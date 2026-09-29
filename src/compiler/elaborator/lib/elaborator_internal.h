#ifndef PULSE_COMPILER_ELABORATOR_INTERNAL_H
#define PULSE_COMPILER_ELABORATOR_INTERNAL_H

#include "elaborator.h"
#include "instruction_builder.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Pulse::Parser
{
    using Engine::LogicVector;

    // --------------------------------------------------------------------------------------------
    // Values and layouts
    // --------------------------------------------------------------------------------------------

    /// How the values of a VHDL type are laid out on a wire (-Ologic).
    struct Layout
    {
        enum class Kind : uint8_t
        {
            Logic,          ///< std_logic: one 01XZ bit
            Boolean,        ///< false = 0, true = 1
            Enumeration,    ///< The position of the literal, in the fewest bits that hold every position
            Integer,        ///< Two's complement (or plain binary for a type without negative values), as wide as the type
            Vector,         ///< A one-dimensional array of std_logic: one bit per element, the rightmost element in bit 0
        };

        Kind kind = Kind::Logic;
        bitWidth_t width = 1;
        bool isSigned = false;              ///< Integers with negative values and `signed` vectors
        const TypeInfo* type = nullptr;     ///< The base type
        int64_t left = 0;                   ///< Vectors: index of the leftmost element
        int64_t right = 0;                  ///< Vectors: index of the rightmost element
        bool ascending = false;             ///< Vectors: `to` (true) or `downto` (false)

        bool isVector() const { return kind == Kind::Vector; }
        /// The bit (0 = least significant) that holds element `index` of a vector.
        int64_t bitOf(int64_t index) const { return ascending ? right - index : index - right; }
        int64_t low() const { return ascending ? left : right; }
        int64_t high() const { return ascending ? right : left; }
        bool contains(int64_t index) const { return index >= low() && index <= high(); }
    };

    /// A value known at elaboration time.
    struct StaticValue
    {
        enum class Kind : uint8_t { Integer, Real, Physical, Enumeration, Vector };

        Kind kind = Kind::Integer;
        int64_t integer = 0;                ///< Integer value, enumeration position, or physical value in base units
        double real = 0.0;
        LogicVector bits{ 0, 0 };           ///< Vectors: one bit per element, the rightmost element in bit 0
        uint64_t dontCare = 0;              ///< Vectors: elements written '-' (only matching operators treat them apart)
        bitWidth_t width = 0;               ///< Vectors: number of elements
        const TypeInfo* type = nullptr;     ///< Its type (a universal type for untyped literals)

        static StaticValue ofInteger(int64_t value, const TypeInfo* type)
        {
            StaticValue v; v.kind = Kind::Integer; v.integer = value; v.type = type; return v;
        }
        static StaticValue ofEnumeration(int64_t position, const TypeInfo* type)
        {
            StaticValue v; v.kind = Kind::Enumeration; v.integer = position; v.type = type; return v;
        }
        static StaticValue ofVector(LogicVector bits, bitWidth_t width, const TypeInfo* type, uint64_t dontCare = 0)
        {
            StaticValue v; v.kind = Kind::Vector; v.bits = bits.range(width); v.width = width; v.type = type; v.dontCare = dontCare; return v;
        }
        /// Integers, enumeration positions and physical values.
        bool isDiscrete() const { return kind == Kind::Integer || kind == Kind::Enumeration || kind == Kind::Physical; }
    };

    /// A discrete range known at elaboration time.
    struct StaticRange
    {
        int64_t left = 0;
        int64_t right = 0;
        bool ascending = true;
        bool enumeration = false;           ///< The values are positions of an enumeration type
        const TypeInfo* type = nullptr;     ///< That enumeration type, when known

        int64_t length() const { return ascending ? (right < left ? 0 : right - left + 1) : (left < right ? 0 : left - right + 1); }
        /// The i-th value from the left.
        int64_t at(int64_t i) const { return ascending ? left + i : left - i; }
    };

    /// An operand after lowering: a value known at elaboration time, or a wire of the blueprint.
    struct Operand
    {
        std::optional<StaticValue> value;
        std::string wire;
        Layout layout;

        bool isStatic() const { return value.has_value(); }
    };

    /// The generic values of a specialization, in the order the entity declares its generics.
    using GenericValues = std::vector<std::pair<const GenericDeclaration*, StaticValue>>;

    // --------------------------------------------------------------------------------------------
    // Design
    // --------------------------------------------------------------------------------------------

    /// What a parent needs to know about the blueprint of one specialization.
    struct Specialization
    {
        Engine::Blueprint* blueprint = nullptr;
        std::string entityName;
        std::unordered_map<std::string, Layout> ports;              ///< Layout of every port, by name
        std::unordered_map<std::string, LogicVector> portDefaults;  ///< Default value of every port, by name
        /// Output ports a process of the specialization drives. Its parent must not drive their actuals in any other way:
        /// a process drives a wire directly, which only works while the wire has no source.
        std::unordered_set<std::string> processDrivenPorts;
    };

    /// Elaborates a whole design: the specialization of every entity the top instantiates, directly or not.
    class DesignElaborator
    {
        const DesignLibrary& m_library;
        const ElaborationOptions& m_options;
        ElaboratedDesign m_design;
        /// Specializations by key (entity, architecture and generic values), each built once.
        std::unordered_map<std::string, std::unique_ptr<Specialization>> m_specializations;
        /// Keys of the specializations being built, outermost first: an instance of one of them is a recursion.
        std::vector<std::string> m_building;

    public:
        DesignElaborator(const DesignLibrary& library, const ElaborationOptions& options);

        /// Elaborates the design from its top entity.
        ElaboratedDesign run(const ASTRoot& linkedDesign);

        /// The specialization of `entity` with `architecture` and `generics`, built on first use.
        /// @param at Node to report a recursive instantiation at.
        const Specialization& specialize(const EntityDeclaration& entity, const ArchitectureDeclaration& architecture,
                                         const GenericValues& generics, const ASTNode& at);

        const DesignLibrary& library() const { return m_library; }
        const ElaborationOptions& options() const { return m_options; }
    };

    // --------------------------------------------------------------------------------------------
    // One architecture
    // --------------------------------------------------------------------------------------------

    /// An object of the architecture that has a wire: a port, a signal, or a variable of a process.
    struct ObjectWire
    {
        enum class Class : uint8_t { Port, Signal, Variable };

        Class objectClass = Class::Signal;
        std::string name;               ///< The VHDL name, for messages
        std::string wire;               ///< The wire of the blueprint
        Layout layout;
        PortMode mode = PortMode::In;   ///< Ports only
        const ASTNode* declaration = nullptr;
        LogicVector defaultValue{ 0, 0 };   ///< The value the wire starts with
    };

    /// Everything that drives one signal or output port of the architecture.
    struct DriverSet
    {
        std::string name;
        std::vector<std::string> processes;     ///< Processes (and process-driven instance ports) that assign it
        std::vector<std::string> others;        ///< Concurrent statements and instance outputs that drive it through sources
        SourceLocation location;
        bool isPort = false;
    };

    /// The loop being unrolled, for `exit` and `next`.
    struct LoopFrame
    {
        std::string label;
        InstructionBuilder::Label exitLabel;    ///< End of the whole loop
        InstructionBuilder::Label nextLabel;    ///< End of the current iteration
    };

    /// The process being lowered.
    struct ProcessContext
    {
        std::string name;
        const ProcessStatement* statement = nullptr;    ///< Null for the process a concurrent assignment stands for
        SourceLocation location;
        InstructionBuilder builder;
        std::vector<LoopFrame> loops;
        /// Signals the process assigns in parts somewhere: every assignment to them goes through a shadow wire.
        std::unordered_set<const ASTNode*> partialTargets;
        /// Shadow wires of the signals the process assigns in parts, by signal declaration.
        std::unordered_map<const ASTNode*, std::string> shadows;
        /// Signals read by the process (for `process(all)` and equivalent processes), in order of first read.
        std::vector<std::string> signalsRead;
        /// Where to also note the signals read, while a `wait until` condition is lowered; null otherwise.
        std::vector<std::string>* readSink = nullptr;
    };

    /// Lowers one architecture of one entity, with given generic values, to a blueprint of logic components.
    ///
    /// The work is split over the elaborator/*.cc files by responsibility: layouts of types (layout.cc), values known at
    /// elaboration time (evaluator.cc), expressions (expressions.cc, builtins.cc), concurrent statements (concurrent.cc),
    /// processes (processes.cc), instances (instances.cc), drivers (drivers.cc) and the symbol table (symbols.cc).
    class UnitElaborator
    {
    public:
        UnitElaborator(DesignElaborator& design, const EntityDeclaration& entity, const ArchitectureDeclaration& architecture,
                       const GenericValues& generics, Engine::Blueprint& blueprint, Specialization& specialization);

        /// Gives every generic without a value its default, in declaration order, and returns them all.
        /// @param at Node to report a generic without any value at.
        GenericValues completeGenerics(const ASTNode& at);

        /// Builds the blueprint.
        void run();

    private:
        DesignElaborator& m_design;
        const DesignLibrary& m_library;
        const EntityDeclaration& m_entity;
        const ArchitectureDeclaration& m_architecture;
        Engine::Blueprint& m_bp;
        Specialization& m_spec;

        /// Values known at elaboration time, by declaration: generics, constants and the parameters of unrolled loops.
        std::unordered_map<const ASTNode*, StaticValue> m_values;
        /// Constants being evaluated, so a constant that refers to itself is reported instead of recursing.
        std::unordered_set<const ASTNode*> m_evaluating;
        /// Ports, signals and variables, by declaration.
        std::unordered_map<const ASTNode*, ObjectWire> m_objects;
        /// Type and subtype declarations visible by name, innermost region last (the architecture, then a process).
        std::vector<std::unordered_map<std::string, const Declaration*>> m_typeScopes;
        /// Every signal and output port that something drives, by declaration.
        std::unordered_map<const ASTNode*, DriverSet> m_drivers;
        /// Event probes built so far, by the wire they watch.
        std::unordered_map<std::string, std::string> m_events;
        /// Constant wires built so far, by value, so equal constants share one wire.
        std::map<std::tuple<uint64_t, uint64_t, bitWidth_t>, std::string> m_constants;
        /// The process being lowered, or null in the architecture body.
        ProcessContext* m_process = nullptr;
        size_t m_nextWire = 0;
        size_t m_nextComponent = 0;
        size_t m_nextProcess = 0;

        // ---- Diagnostics (elaborator.cc) ---------------------------------------------------------
        [[noreturn]] void fail(const std::string& message, const ASTNode& at) const;
        /// Reports a construct the elaborator does not lower yet: "<what> is not supported yet".
        [[noreturn]] void unsupported(const std::string& what, const ASTNode& at) const;

        // ---- The unit (elaborator.cc) -------------------------------------------------------------
        void declarePorts();
        void elaborateDeclarations(const std::vector<DeclarationPtr>& declarations, bool inProcess);
        void declareSignal(const SignalDeclaration& decl);
        void declareConstant(const ConstantDeclaration& decl);
        void elaborateStatement(const Statement& statement);

        // ---- Layouts (layout.cc) ------------------------------------------------------------------
        /// The layout of a resolved type; `spec` is the subtype indication it comes from, used for bounds that depend on
        /// generics (the analysis could not know them).
        Layout layoutOf(const SemanticType& type, const TypeSpec* spec, const ASTNode& at);
        /// The layout of an expression's type, from the analysis; vectors whose bounds analysis did not know get
        /// `width` elements when it is given.
        Layout layoutOfExpression(const Expression& expr, std::optional<bitWidth_t> width = std::nullopt);
        /// The layout of a scalar type (no bounds needed).
        Layout scalarLayout(const TypeInfo& type, const ASTNode& at);
        /// A vector layout of `width` elements, `width-1 downto 0`.
        Layout vectorLayout(const TypeInfo& type, bitWidth_t width, bool isSigned) const;
        /// The type the analysis gave an expression.
        const SemanticType& typeOf(const Expression& expr) const;
        /// The bounds of the array type named by `spec` (its index constraint, or the one of the subtype or type it names).
        std::optional<StaticRange> arrayBounds(const TypeSpec& spec, const ASTNode& at);
        /// The value an object of the layout starts with when its declaration gives none: the leftmost value of its
        /// subtype (LRM 6.4.2.3), 'U' (X) for std_logic.
        LogicVector defaultValue(const Layout& layout, const SemanticType& type) const;
        bool isStdLogic(const TypeInfo* type) const;
        bool isBoolean(const TypeInfo* type) const;

        // ---- Values known at elaboration time (evaluator.cc) --------------------------------------
        std::optional<StaticValue> evaluate(const Expression& expr);
        /// The value of an expression that must be known at elaboration time. `layout` is the layout of what the value is
        /// for: an aggregate such as `(others => '0')` takes its bounds from it when its type has none of its own.
        StaticValue requireStatic(const Expression& expr, const std::string& what, const Layout* layout = nullptr);
        int64_t requireInteger(const Expression& expr, const std::string& what);
        std::optional<StaticValue> evaluateName(const SymbolExpr& expr);
        std::optional<StaticValue> evaluateUnary(const UnaryOpExpr& expr);
        std::optional<StaticValue> evaluateBinary(const BinaryOpExpr& expr);
        std::optional<StaticValue> evaluateCall(const FunctionCallExpr& expr);
        std::optional<StaticValue> evaluateAttribute(const AttributeExpr& expr);
        std::optional<StaticValue> evaluateString(const StringLiteralExpr& expr);
        /// A static aggregate of std_logic elements; `layout` gives its bounds when its type does not.
        std::optional<StaticValue> evaluateAggregate(const AggregateExpr& expr, const Layout* layout);
        std::optional<StaticValue> evaluateBuiltin(const CallTarget& callee, const std::vector<StaticValue>& arguments, const Expression& at);
        /// A discrete range: `a to b`, `a downto b`, `x'range`, `x'reverse_range` or a discrete type name.
        StaticRange evaluateRange(const Expression& range, const std::string& what);
        /// The bits of a static value laid out as `layout`.
        LogicVector bitsOf(const StaticValue& value, const Layout& layout, const ASTNode& at) const;
        /// The -Ologic value of a std_logic literal position ('0' and 'L' -> 0, '1' and 'H' -> 1, 'Z' -> Z, else X).
        LogicVector logicOf(int64_t position) const;

        // ---- Wires and components (expressions.cc) ------------------------------------------------
        std::string newWire(bitWidth_t width, const std::string& hint = "t", LogicVector defaultValue = LogicVector::HighZ());
        void addComponent(const std::string& hint, std::unique_ptr<Engine::ComponentInstance> component);
        std::string constantWire(LogicVector bits, bitWidth_t width);
        /// The wire of an operand, as `layout` (a static value becomes a constant; a wire must already have that width).
        std::string materialize(const Operand& operand, const Layout& layout, const ASTNode& at);
        std::string slice(const std::string& wire, bitWidth_t high, bitWidth_t low);
        std::string concat(const std::string& high, const std::string& low, bitWidth_t width);
        std::string gate(Engine::BinaryOp op, const std::string& a, const std::string& b, bitWidth_t width);
        std::string invert(const std::string& a, bitWidth_t width);
        std::string compare(Engine::CompareOp op, bool isSigned, const std::string& a, const std::string& b);
        std::string shift(Engine::ShiftOp op, const std::string& in, const std::string& amount6, bitWidth_t width);
        std::string shiftBy(Engine::ShiftOp op, const std::string& in, bitWidth_t amount, bitWidth_t width);
        /// `whenTrue` when the 1-bit `condition` is '1', otherwise `whenFalse` (two tri-state buffers on one wire).
        std::string select(const std::string& condition, const std::string& whenTrue, const std::string& whenFalse, bitWidth_t width);
        /// Extends (zero or sign) or truncates to `to` bits, keeping the least significant bits.
        std::string resizeWire(const std::string& wire, bitWidth_t from, bitWidth_t to, bool isSigned);
        /// The 'event of an object's wire (one probe per wire).
        std::string eventOf(const std::string& wire);
        /// Connects `from` to `to` as a source (a Join).
        void join(const std::string& from, const std::string& to);

        // ---- Expressions (expressions.cc) ---------------------------------------------------------
        Operand lower(const Expression& expr, const Layout* expected = nullptr);
        /// Lowers and materializes as `layout`.
        std::string lowerTo(const Expression& expr, const Layout& layout);
        /// A 1-bit wire that is '1' when the condition (boolean, or std_logic through `??`) holds.
        std::string lowerCondition(const Expression& condition);
        Operand lowerName(const SymbolExpr& expr);
        Operand lowerCall(const FunctionCallExpr& expr);
        Operand lowerIndexOrSlice(const FunctionCallExpr& expr, const Operand& prefix);
        Operand lowerAttribute(const AttributeExpr& expr);
        Operand lowerAggregate(const AggregateExpr& expr, const Layout* expected);
        Operand lowerConversion(const FunctionCallExpr& expr, const Operand& argument);
        /// The object a name designates, or nullptr when it is not a port, signal or variable of this unit.
        const ObjectWire* objectOf(const Expression& name) const;
        /// Notes that the process being lowered reads an object (for `process(all)` and `wait until`).
        void noteRead(const ObjectWire& object);
        Operand wireOperand(std::string wire, Layout layout) const;

        // ---- Operators (builtins.cc) --------------------------------------------------------------
        Operand lowerUnary(const UnaryOpExpr& expr, const Layout* expected);
        Operand lowerBinary(const BinaryOpExpr& expr, const Layout* expected);
        Operand lowerBuiltinCall(const CallTarget& callee, const Expression& call, const std::vector<const Expression*>& arguments);
        Operand lowerLogical(Engine::BinaryOp op, bool negate, const Operand& l, const Operand& r, const Expression& at);
        Operand lowerReduction(UnaryOperator op, const Operand& operand, const Expression& at);
        Operand lowerArithmetic(BinaryOperator op, const Operand& l, const Operand& r, const Expression& at);
        Operand lowerComparison(BinaryOperator op, const Operand& l, const Operand& r, const Expression& at);
        Operand lowerShift(const std::string& name, const Operand& l, const Operand& r, const Expression& at);
        Operand lowerMatching(BinaryOperator op, const Operand& l, const Operand& r, const Expression& at);
        Operand lowerConcatenation(const Operand& l, const Operand& r, const Expression& at);
        Operand lowerEdge(const Expression& signal, bool rising, const Expression& at);
        /// A numeric operand (integer or vector) as a wire of `width` bits, extended by its own signedness.
        std::string numericWire(const Operand& operand, bitWidth_t width, const ASTNode& at);

        // ---- Concurrent statements (concurrent.cc) ------------------------------------------------
        void elaborateSignalAssignment(const SignalAssignment& assignment);
        void elaborateWithClause(const WithClause& with);
        /// Drives the target of a concurrent assignment with the value `wire` (the whole target or a part of it).
        void driveTarget(const Expression& target, const std::string& wire, const std::string& driver);
        /// The value of a conditional value (`a when c else b ...`) with a final `else`, built from tri-state buffers.
        std::string lowerConditional(const WhenElseExpr& value, const Layout& layout);
        /// The 1-bit wire that is '1' when `selector` matches one of the choices of a list.
        std::string lowerChoiceMatch(const Operand& selector, const ChoiceListExpr& choices, bool matching);
        /// Whether a conditional value can reach no assignment (no final `else`, or `unaffected`), so it needs a process.
        static bool keepsValue(const Expression& value);
        /// A static slice or element target: the part of the object's wire it designates.
        struct TargetPart
        {
            const ObjectWire* object = nullptr;
            bitWidth_t high = 0;        ///< Bits of the part in the object's wire
            bitWidth_t low = 0;
            bool whole = true;
            Layout layout;              ///< Layout of the part (the object's own when whole)
        };
        TargetPart targetPart(const Expression& target);

        // ---- Processes (processes.cc) -------------------------------------------------------------
        void elaborateProcess(const ProcessStatement& process);
        /// A concurrent assignment that keeps its value when no branch assigns it: lowered as the process it stands for.
        void elaborateEquivalentProcess(const SignalAssignment& assignment);
        void lowerSequence(const std::vector<StatementPtr>& statements);
        void lowerSequential(const Statement& statement);
        void lowerSequentialAssignment(const Expression& target, const Expression& value, bool signal, const ASTNode& at);
        void assignTarget(const Expression& target, const std::string& wire, bool signal, const ASTNode& at);
        void lowerIf(const IfStatement& statement);
        void lowerCase(const CaseStatement& statement);
        /// VHDL-2008 `with s select t <= ...;` in a process: a case statement over signal assignments.
        void lowerSelected(const WithClause& with);
        void lowerForLoop(const ForLoopStatement& loop);
        void lowerExitOrNext(const std::string& label, const Expression* condition, bool exit, const ASTNode& at);
        void lowerWait(const WaitStatement& statement);
        std::string variableWire(const VariableDeclaration& decl);
        /// The wire holding the value a process is giving a signal it assigns in parts.
        std::string shadowOf(const ObjectWire& signal);
        /// The whole signals a list of names designates (a sensitivity list or a `wait on`).
        std::vector<std::string> sensitivityOf(const std::vector<ExpressionPtr>& names);
        /// The trigger wires of a process waiting on these signals: the outputs of their event probes.
        std::vector<std::string> triggersOf(const std::vector<std::string>& wires);

        // ---- Instances (instances.cc) -------------------------------------------------------------
        void elaborateInstance(const ComponentInstantiation& instance);
        /// Gives the wire of a port or signal the value it starts with.
        void setInitialValue(const ObjectWire& object, LogicVector value);
        GenericValues genericValuesOf(const ComponentInstantiation& instance, const ComponentDeclaration& component,
                                      const EntityDeclaration& entity);

        // ---- Drivers (drivers.cc) -----------------------------------------------------------------
        /// Notes that `driver` drives the signal or port `object`; `fromProcess` when it drives it directly (a process).
        void recordDriver(const ObjectWire& object, const std::string& driver, bool fromProcess, const ASTNode& at);
        /// A signal assigned by a process must have that process as its only driver.
        void checkDrivers();

        // ---- Symbol table (symbols.cc) ------------------------------------------------------------
        void fillSymbols();
        Engine::SignalSymbol symbolOf(const Layout& layout) const;
    };

    /// A short description of a static value for messages and specialization keys.
    std::string describeValue(const StaticValue& value);

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_ELABORATOR_INTERNAL_H
