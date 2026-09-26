#ifndef PULSE_COMPILER_ANALYZER_INTERNAL_H
#define PULSE_COMPILER_ANALYZER_INTERNAL_H

#include "analyzer.h"
#include "analyzer_scope.h"
#include "depth_guard.h"
#include "node_dispatch.h"
#include "operator_helpers.h"
#include "operator_rules.h"
#include "subprogram_info.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Pulse::Parser
{
    /// A discrete range such as `7 downto 0` or `v'range`. Bounds are absent for a dynamic range
    /// (e.g. a slice `v(i downto 0)`), whose direction and index type are still known.
    struct RangeInfo
    {
        SemanticType type;                  ///< Discrete type of the range values (may be universal integer)
        bool ascending = false;             ///< `to` (true) or `downto` (false)
        std::optional<Bounds> bounds;       ///< Static bounds; nullopt when they depend on non-constant values
        bool dependsOnGenerics = false;     ///< The bounds are constant for an instance but unknown here (a generic is involved)
    };

    /// The types of the two operands of a binary operator, after operands that need a context (strings,
    /// aggregates, character literals) were typed from their partner operand or from the surrounding context.
    struct OperandTypes
    {
        SemanticType left;      ///< Type of the left operand
        SemanticType right;     ///< Type of the right operand
    };

    /// Handles to the few predefined types the analyzer itself has to name: conditions are boolean, timeouts are
    /// time, loop parameters are integer, severities are severity_level. They are looked up by name exactly once,
    /// right after the prelude is loaded; everywhere else types are compared by TypeInfo identity.
    struct PreludeTypes
    {
        const TypeInfo* boolean = nullptr;      ///< Result of comparisons, type of conditions
        const TypeInfo* stdLogic = nullptr;     ///< Also accepted as a condition (VHDL-2008)
        const TypeInfo* time = nullptr;         ///< Type of `wait for` timeouts
        const TypeInfo* integer = nullptr;      ///< Type of for-loop parameters over literal ranges
        const TypeInfo* severity = nullptr;     ///< Type of `severity` levels
    };

    /// The object a name is built on: `s` in `s`, `s(3)`, `s(7 downto 4)` and `s.f(1)`, or an external name.
    struct RootObject
    {
        const Expression* node = nullptr;       ///< The SymbolExpr or ExternalNameExpr at the root; null when the name has no root
        const Symbol* symbol = nullptr;         ///< Its symbol, when it is a SymbolExpr that names one
        std::string name;                       ///< Name for messages
        SymbolKind kind = SymbolKind::Label;    ///< What the root is; Label when it is not an object
        std::optional<PortMode> mode;
        size_t objectId = 0;
        size_t depth = 0;                       ///< Scope index where the object is declared
        bool whole = false;                     ///< The name is the object itself, not a part of it
        bool isPort = false;
        bool isParameter = false;

        explicit operator bool() const { return node != nullptr; }
    };

    /// How an association list is being checked; decides which formals must be associated.
    enum class AssociationKind { PortMap, GenericMap, Call };

    /// One formal matched with the actual(s) associated to it.
    struct Binding
    {
        size_t formal = 0;                              ///< Index into the formal list
        const Expression* actual = nullptr;            ///< The actual expression (an OpenExpr for `open`)
        const Expression* association = nullptr;       ///< The whole association (for locations)
        const Expression* formalPart = nullptr;        ///< The formal as written; differs from the plain name for partial associations
        const FunctionCallExpr* conversion = nullptr;  ///< `f(formal) => actual`: the conversion applied to the formal
        bool partial = false;                          ///< Only an element, a slice or a field of the formal is associated
    };

    /// Result of matching actuals to formals, without looking at any type.
    struct AssociationResult
    {
        std::string problem;                            ///< Empty when the association is well formed
        const ASTNode* at = nullptr;                    ///< Node the problem is about
        std::vector<Binding> bindings;
        std::vector<bool> associated;                   ///< Per formal: has at least one binding

        bool ok() const { return problem.empty(); }
    };

    /// The analysis state of a design library: every design file analyzed into it, one at a time.
    ///
    /// The design units of a file are analyzed in textual order, each against the units analyzed before it: an entity
    /// resolves its generics and ports and joins the library, and an architecture is validated against the entity it
    /// belongs to, which must already be in the library. Every unit is checked against the VHDL typing rules: names
    /// resolve through nested scopes, every expression gets a SemanticType, every assignment and port connection is
    /// checked for compatibility, and signals with several drivers are rejected. Every node the parser can build has a
    /// handler; any violation throws ast_semantic_error with a message that describes the problem. What analysis cannot
    /// know, the entity a component instance stands for, is left to the linker, which reads the components and the
    /// interface types recorded here.
    ///
    /// The class is deliberately one type with many small member functions, spread over the
    /// analyzer/*.cc files, one per responsibility (each group below names its file). Node kinds are
    /// routed to their handler through NodeDispatch tables, so supporting a new construct means writing a
    /// handler and adding one registration line.
    ///
    /// The object holds the mutable state of the library (scope stack, type arena, entities, driver table). It is neither
    /// copyable nor movable because the dispatch tables capture `this`.
    class AnalyzerContext
    {
    public:
        /// Builds the dispatch tables and loads the prelude, which every file analyzed afterwards shares.
        AnalyzerContext();
        AnalyzerContext(const AnalyzerContext&) = delete;
        AnalyzerContext& operator=(const AnalyzerContext&) = delete;

        /// Analyzes the design units of one design file in textual order: an entity joins the library, an architecture is
        /// checked against the entity of the library it belongs to.
        /// @throws ast_semantic_error on the first violation found.
        void analyze(const ASTRoot& root);

        /// The component an analyzed instance instantiates, or nullptr.
        const ComponentDeclaration* componentOf(const ComponentInstantiation& instance) const;
        /// The type resolved for a generic or a port of an analyzed entity or component, or nullptr.
        const SemanticType* interfaceType(const Declaration& genericOrPort) const;

    private:
        /// An entity of the library: its generics and ports, resolved once in a scope that only sees the predefined types,
        /// and the names of the architectures analyzed for it so far.
        struct LibraryEntity
        {
            const EntityDeclaration* declaration = nullptr;
            std::vector<FormalInfo> generics;
            std::vector<FormalInfo> ports;
            std::unordered_set<std::string> architectures;
        };

        /// Stack of declarative regions, innermost last. Index 0 is the prelude scope; each entity, architecture, process,
        /// subprogram body and for-loop pushes one more.
        std::vector<Scope> m_scopes;
        /// Arena owning every TypeInfo created by declarations. Symbols and SemanticTypes only hold raw pointers into it,
        /// so addresses must stay stable for the whole life of the library (hence unique_ptr).
        std::vector<std::unique_ptr<TypeInfo>> m_types;
        /// Arena owning every subprogram declared by the design; symbols only hold pointers to them.
        std::vector<std::unique_ptr<SubprogramInfo>> m_subprograms;
        /// Entities of the library by name, added by analyzeEntity() once they are analyzed.
        std::unordered_map<std::string, LibraryEntity> m_entities;
        /// Names of the entities the file being analyzed declares, to tell an architecture written above its entity from an
        /// architecture of an unknown entity.
        std::unordered_set<std::string> m_fileEntities;
        /// The component each analyzed instance instantiates, for the linker.
        std::unordered_map<const ComponentInstantiation*, const ComponentDeclaration*> m_instanceComponents;
        /// The type of every generic and port of the analyzed entities and components, for the linker.
        std::unordered_map<const Declaration*, SemanticType> m_interfaceTypes;
        /// Predefined types the analyzer needs to name; set by bindPreludeTypes().
        PreludeTypes m_std;
        /// Predefined operator typing rules (pure functions over SemanticType).
        OperatorRules m_rules;
        /// True while the prelude declarations are being analyzed; lets declareType() attach the traits VHDL text cannot express.
        bool m_loadingPrelude = false;
        /// Identity given to the next object (signal, variable, constant, port ...) declared.
        size_t m_nextObjectId = 0;
        /// Subprogram chosen for every call expression and every operator that resolved to a user-declared function.
        std::unordered_map<const ASTNode*, const SubprogramInfo*> m_resolvedCalls;

        /// Design unit node kind -> analyze* handler.
        NodeDispatch<DesignUnit, void> m_units;
        /// Expression node kind -> typeOf* handler.
        NodeDispatch<Expression, SemanticType, const SemanticType*> m_expressions;
        /// Type definition node kind -> define* handler that fills a fresh TypeInfo.
        NodeDispatch<TypeDefinition, void, TypeInfo&> m_definitions;
        /// Declaration node kind -> declare* handler.
        NodeDispatch<Declaration, void> m_declarations;
        /// Statement kinds legal in an architecture body -> handler.
        NodeDispatch<Statement, void> m_concurrent;
        /// Statement kinds legal in a process or subprogram body -> handler.
        NodeDispatch<Statement, void> m_sequential;

        // ---- Statement context -------------------------------------------------------------------
        /// The architecture being analyzed, or nullptr in an entity. Attribute specifications of the architecture name it.
        const ArchitectureDeclaration* m_architecture = nullptr;
        /// Attributes specified for the architecture being analyzed, so that none is specified twice.
        std::unordered_set<std::string> m_architectureAttributes;
        /// Labels of the loops enclosing the statement being analyzed, outermost first (unlabeled loops store "").
        /// empty() means `exit`/`next` are illegal here.
        std::vector<std::string> m_loops;
        /// The process whose body is being analyzed, or nullptr in the architecture body. Used to reject `wait` in
        /// a process that has a sensitivity list.
        const ProcessStatement* m_process = nullptr;
        /// The subprogram whose body is being analyzed, or nullptr outside of any body.
        SubprogramInfo* m_subprogram = nullptr;
        /// The subprogram body being analyzed is declared inside a process, so it may assign that process's signals.
        bool m_bodyInProcess = false;
        /// Scope index of the outermost scope of the subprogram body being analyzed; anything declared below it is
        /// declared outside the subprogram.
        size_t m_bodyDepth = 0;
        /// Depth of the expression currently being typed (guards the stack against absurdly long expressions).
        mutable size_t m_expressionDepth = 0;

        /// One source that drives a signal: a concurrent assignment, a process or an instance output.
        struct Driver
        {
            /// Identity of the driving statement (address of its AST node); two records with the same source are one driver.
            const void* source = nullptr;
            /// Text for the diagnostic, e.g. "process 'p1'".
            std::string description;
            /// Drives the entire signal rather than an element or a slice of it.
            bool wholeSignal = false;
            /// Where the first assignment of this source appears.
            SourceLocation location;
        };
        /// Everything known about one signal for the multiple-driver rule.
        struct DriverSet
        {
            std::string name;
            SemanticType type;
            std::vector<Driver> drivers;
        };
        /// Drivers recorded so far for the current architecture, by object identity.
        std::unordered_map<size_t, DriverSet> m_drivers;
        /// Source that signal assignments analyzed right now are attributed to (set by beginDriverSource()).
        const void* m_driverSource = nullptr;
        /// Diagnostic text for m_driverSource.
        std::string m_driverDescription;

        /// An attribute specification for labels can only be applied once the statements that declare them are analyzed.
        struct PendingLabelSpec
        {
            const AttributeSpecification* specification = nullptr;
            std::string label;
            AttributeValue value;
            size_t depth = 0;               ///< Scope index of the region the specification appears in
            bool matched = false;           ///< A label of that name was declared
        };
        std::vector<PendingLabelSpec> m_pendingLabelSpecs;

        /// A procedure called from a statement of a process or of an architecture (calls inside subprograms are kept in
        /// SubprogramInfo::callees). Checked against the wait rules once every body of the region is known.
        struct StatementCall
        {
            const ProcessStatement* process = nullptr;      ///< The calling process, or null for a concurrent call
            const SubprogramInfo* callee = nullptr;
            SourceLocation location;
        };
        std::vector<StatementCall> m_statementCalls;

        /// A subprogram declared without a body; its region must provide one before the region ends.
        struct PendingBody
        {
            const SubprogramInfo* info = nullptr;
            size_t depth = 0;               ///< Scope index of the region that declared it
            SourceLocation location;
        };
        std::vector<PendingBody> m_pendingBodies;

        // ---- Diagnostics and scopes (scopes.cc) -----------------------------------------------------

        /// Throws ast_semantic_error(message) located at `location`. Every diagnostic of the analyzer goes through here.
        [[noreturn]] void fail(const std::string& message, const SourceLocation& location) const;
        /// Same as above, located at the source position of `node`.
        [[noreturn]] void fail(const std::string& message, const ASTNode& node) const;

        /// Opens a new innermost declarative region.
        void pushScope();
        /// Closes the innermost region, forgetting everything declared in it.
        void popScope();
        /// Allocates a TypeInfo in the arena (`cls` is a placeholder the type definition handler overwrites).
        TypeInfo& newType(const std::string& name, TypeClass cls);
        /// A fresh object identity.
        size_t newObjectId() { return ++m_nextObjectId; }

        /// Adds a symbol to the innermost scope; rejects homographs of names already declared there.
        void declare(const std::string& name, Symbol symbol, const ASTNode& node);
        void declare(const std::string& name, Symbol symbol, const SourceLocation& location);
        /// Looks a name up from the innermost scope outwards; nullptr when it is not declared.
        const Symbol* find(const std::string& name) const;
        /// The innermost region's symbol of that name, for adding to it after it was declared; nullptr when there is none.
        Symbol* findInCurrentScope(const std::string& name);
        /// Every subprogram overload visible under `name`, innermost declarations first. A name that is not a subprogram
        /// hides the outer overloads.
        std::vector<const SubprogramInfo*> visibleSubprograms(const std::string& name) const;

        /// Visible enumeration types declaring `literal` (an outer type is hidden by an inner declaration of its name).
        std::vector<const TypeInfo*> enumerationOwners(const std::string& literal) const;
        /// The owner of `literal` chosen by the expected type, or the only visible owner; nullptr when none/ambiguous.
        const TypeInfo* pickEnumerationOwner(const std::string& literal, const SemanticType* expected) const;
        /// Finds a visible physical unit by name (`ns`) together with the type that owns it.
        std::optional<UnitRef> findUnit(const std::string& unit) const;

        // ---- Prelude (prelude.cc) --------------------------------------------------------------------

        /// Parses the embedded VHDL prelude and declares its types in the base scope (scope 0), then binds m_std.
        void loadPrelude();
        /// Looks up boolean, std_logic, time, integer and severity_level in the base scope and builds m_rules.
        void bindPreludeTypes();
        /// Sets the properties VHDL text cannot express on a predefined type: std_logic is resolved and the three
        /// IEEE vectors get their VectorFamily.
        void applyPreludeTraits(TypeInfo& info) const;

        // ---- Type specifications (type_specs.cc) -----------------------------------------------------

        /// Resolves a type or subtype name to its SemanticType (base type plus the constraint the name stands for).
        /// @throws ast_semantic_error if the name is unknown or does not name a type.
        SemanticType resolveTypeName(const std::string& name, const ASTNode& node);
        /// Resolves a subtype indication: a resolution, a type name and optionally an index constraint
        /// (`std_logic_vector(7 downto 0)`) or a range constraint (`integer range 0 to 15`).
        SemanticType resolveTypeSpec(const TypeSpec& spec);

        /// Like resolveTypeSpec, but the type of a signal/port/variable must be fully constrained.
        SemanticType resolveObjectType(const TypeSpec& spec, const std::string& what, const std::string& name);
        /// Applies the index constraint of `spec` to an unconstrained array type (checks dimension count, static bounds,
        /// non-empty ranges and that the bounds lie inside the index type). Bounds that depend on generics leave the
        /// array constrained with unknown bounds.
        SemanticType constrainArray(const SemanticType& base, const TypeSpec& spec);
        /// Applies the range constraint of `spec` to an integer, enumeration or physical type; the range must be static,
        /// non-empty and inside the range of `base`.
        SemanticType constrainScalar(const SemanticType& base, const TypeSpec& spec);
        /// Range constraint on a real type: the bounds are real numbers rather than discrete values.
        SemanticType constrainReal(const SemanticType& base, const TypeSpec& spec);
        SemanticType constrainPhysical(const SemanticType& base, const TypeSpec& spec);

        // ---- Resolution functions (resolution.cc) ----------------------------------------------------

        /// Applies the resolution indication of `spec` (`f t` or `(f) t`) to `type`: `f` must be a function from an array of
        /// the type to the type; the subtype becomes resolved.
        void applyResolution(SemanticType& type, const Expression& resolution, const ASTNode& at);
        /// True when `f` is a function named `name` that resolves `type` (one parameter, an unconstrained array of the base
        /// type, and the base type as result).
        bool isResolutionFunction(const std::string& name, const SemanticType& type) const;

        // ---- Ranges (ranges.cc) ----------------------------------------------------------------------

        /// True when `expr` is a discrete range: `a to b`, `a downto b`, `x'range`, `x'reverse_range`, a discrete type name or
        /// a subtype indication with a range.
        bool isDiscreteRange(const Expression& expr) const;
        /// Any discrete range; `context` types the bounds when known.
        RangeInfo analyzeRange(const Expression& range, const SemanticType* context = nullptr);
        /// The `a to b` / `a downto b` case of analyzeRange(): types both bounds, checks they are discrete and agree, folds them.
        RangeInfo analyzeExplicitRange(const BinaryOpExpr& range, const SemanticType* context);
        /// The `x'range` case of analyzeRange(); returns nullopt when `range` is not such an attribute.
        std::optional<RangeInfo> analyzeRangeAttribute(const Expression& range);
        /// A discrete type name or a subtype indication used as a range: its own range.
        std::optional<RangeInfo> analyzeTypeRange(const Expression& range);
        /// Returns the bounds of `range`, or reports that `what` needs bounds constant at analysis time.
        Bounds requireStaticBounds(const RangeInfo& range, const Expression& node, const std::string& what);
        /// The bounds of an index constraint: known, or unknown because they depend on generics. Reports every other case.
        struct IndexConstraint
        {
            std::vector<Bounds> dims;
            bool unknown = false;
        };
        IndexConstraint analyzeIndexConstraint(const std::vector<ExpressionPtr>& ranges, const TypeInfo& arrayInfo);

        // ---- Constant folding (constants.cc) ---------------------------------------------------------

        /// Constant folding: the value of `expr` when it is known at analysis time (literals, constants, arithmetic,
        /// physical quantities, attributes of constrained types), else nullopt. Never reports errors: callers type-check the
        /// expression first with exprType(). `expected` disambiguates enumeration literals.
        std::optional<ConstValue> fold(const Expression& expr, const SemanticType* expected = nullptr);
        /// Integer, real and character literals.
        std::optional<ConstValue> foldLiteral(const Expression& expr, const SemanticType* expected);
        /// Constants with a known value, enumeration literals and bare physical units.
        std::optional<ConstValue> foldSymbol(const SymbolExpr& symbol, const SemanticType* expected);
        /// `+`, `-`, `abs` on numbers and `not` on booleans. Gives up (nullopt) on overflow.
        std::optional<ConstValue> foldUnary(const UnaryOpExpr& expr, const SemanticType* expected);
        /// Arithmetic on integers, reals and physical quantities, relational operators and boolean logic. Overflow and
        /// division by zero give nullopt.
        std::optional<ConstValue> foldBinary(const BinaryOpExpr& expr, const SemanticType* expected);
        /// `'length 'left 'right 'high 'low` of constrained arrays and scalar types, and values of user attributes.
        std::optional<ConstValue> foldAttribute(const AttributeExpr& expr);
        /// `10 ns`: the magnitude scaled to base units; nullopt when it is not a whole number of base units or overflows.
        std::optional<ConstValue> foldPhysical(const PhysicalLiteralExpr& expr);
        /// Folds `expr` and returns its integer value, or reports that `what` must be a static integer expression.
        int64_t requireStaticInteger(const Expression& expr, const std::string& what, const SemanticType* expected = nullptr);

        // ---- Type declarations (type_decls.cc, type_defs.cc) -----------------------------------------

        /// Analyzes `type name is <definition>;`: builds a fresh TypeInfo through the handler for the definition kind and
        /// declares the name (after the definition, so a type cannot refer to itself).
        void declareType(const TypeDeclaration& decl);
        /// Analyzes `subtype name is <indication>;`: declares a name for an existing type plus a constraint.
        void declareSubtype(const SubtypeDeclaration& decl);
        /// Integer or real type from `range a to b`; the class follows the literals used for the bounds.
        void defineNumeric(const NumericTypeDefinition& def, TypeInfo& info);
        /// Enumeration type: rejects duplicate literals and literals that clash with existing names in the region.
        void defineEnumeration(const EnumeratedTypeDefinition& def, TypeInfo& info);
        /// Physical type: a base range plus the unit table, each unit a whole multiple of an earlier one (`ps = 1000 fs`).
        void definePhysical(const PhysicalTypeDefinition& def, TypeInfo& info);
        /// `array (0 to 3) of T`: static index ranges and a constrained element type.
        void defineConstrainedArray(const ArrayTypeDefinition& def, TypeInfo& info);
        /// `array (natural range <>) of T`: index types must be discrete.
        void defineUnconstrainedArray(const UnconstrainedArrayTypeDefinition& def, TypeInfo& info);
        /// Record type: unique fields with constrained types; the record is resolved only if every field is.
        void defineRecord(const RecordTypeDefinition& def, TypeInfo& info);
        /// Bounds of a numeric type's `range a to b`; sets `cls` to Integer or Real and requires constant, non-empty bounds.
        ScalarRange analyzeScalarRange(const Expression& range, TypeClass& cls, const ASTNode& node);
        /// Resolves an array element or record field type, which must be fully constrained.
        SemanticType requireElementType(const TypeSpec& spec, const std::string& arrayName);

        // ---- Declarations (declarations.cc) ----------------------------------------------------------

        /// Analyzes a declarative part in source order (so a name must be declared before it is used); afterwards every
        /// subprogram declared in it must have a body.
        void analyzeDeclarations(const std::vector<DeclarationPtr>& declarations);
        /// Signal: constrained type, optional constant initial value.
        void declareSignal(const SignalDeclaration& decl);
        /// Constant: its value must be static and is stored for constant folding; an unconstrained array type takes its
        /// index range from the value.
        void declareConstant(const ConstantDeclaration& decl);
        /// Variable (process and subprogram declarative parts): constrained type, optional constant initial value.
        void declareVariable(const VariableDeclaration& decl);
        /// Component: must match the entity of the same name generic by generic and port by port (name, mode, type);
        /// its resolved formals are stored for later maps.
        void declareComponent(const ComponentDeclaration& decl);
        void declareLibraryClause(const LibraryClause& decl);   ///< Library clauses are accepted and have no effect yet.
        void declareUseClause(const UseClause& decl);           ///< Use clauses are accepted and have no effect yet.
        /// A declaration that only makes sense inside its own list (a port, a generic, a parameter) found in a declarative part.
        template <typename Node>
        void misplacedDeclaration(const Node& decl);
        /// The initial value of an object must be constant (no signal, port or variable reads) and assignable to `target`.
        void checkInitialValue(const Expression& init, const SemanticType& target, const std::string& what);

        // ---- Generics and ports (generics.cc) --------------------------------------------------------

        /// Resolves the generics of an entity or component and declares each as a generic constant in the current scope.
        /// A default must be static and assignable. The type of each is recorded for the linker.
        std::vector<FormalInfo> resolveGenerics(const std::vector<std::unique_ptr<GenericDeclaration>>& generics);
        /// Resolves the ports of an entity or component (duplicates are errors, defaults must be static and assignable).
        /// The type of each is recorded for the linker.
        std::vector<FormalInfo> resolvePorts(const std::vector<std::unique_ptr<PortDeclaration>>& ports, const std::string& owner);
        /// Declares already resolved generics as constants of the current scope.
        void declareGenericSymbols(const std::vector<FormalInfo>& generics);
        /// Declares already resolved ports as signals (with their mode) in the current scope.
        void declarePortSymbols(const std::vector<FormalInfo>& ports);

        // ---- Association lists (associations.cc) -----------------------------------------------------

        /// Matches the associations of a map or call with the formals: positional before named, every formal at most once
        /// (unless associated element by element), every formal without a default associated. Looks at no types.
        /// `owner` names what is being connected or called for the messages ("instance 'u1'", "'f'"); `at` locates problems of
        /// an empty list.
        AssociationResult matchAssociations(const std::vector<FormalInfo>& formals, const std::vector<const Expression*>& associations,
                                            AssociationKind kind, const std::string& owner, const ASTNode& at);
        /// The name a formal designator is built on (`p` in `p`, `p(3)`, `p.f`, `conv(p)`), or nullptr.
        const SymbolExpr* formalRoot(const Expression& formal) const;
        /// Checks one matched association: the actual's type against the formal's (through the conversion when there is one) and
        /// what the formal's class and mode ask of the actual (an output needs a writable name, a signal formal a signal ...).
        /// Has no side effects, so overload resolution can use it to try a candidate.
        void checkBinding(const Binding& binding, const FormalInfo& formal, AssociationKind kind, const std::string& what);
        /// The type of the part of `formal` a formal part designates: the formal itself, an element, a slice or a field of it.
        SemanticType formalPartType(const FormalInfo& formal, const Expression& part);
        /// The type a conversion applied to a formal part produces (`to_integer(p) => n`): a type conversion or a function of one
        /// parameter of the part's type.
        SemanticType conversionResultType(const FunctionCallExpr& conversion, const SemanticType& partType, const SemanticType* expected);
        /// The associations of a list as plain pointers.
        static std::vector<const Expression*> associationsOf(const std::vector<ExpressionPtr>& list);
        /// Checks that the element-by-element associations of one formal cover it exactly once, when that can be decided.
        std::string partialCoverageProblem(const FormalInfo& formal, const std::vector<const Binding*>& parts);
        /// Which formal (by index) a formal part designates, and how; sets `problem` when it designates none.
        struct Designator
        {
            size_t index = 0;
            bool partial = false;
            const FunctionCallExpr* conversion = nullptr;
        };
        std::optional<Designator> designate(const std::vector<FormalInfo>& formals, const Expression& formalPart, std::string& problem) const;

        // ---- Expression typing (expr.cc) -------------------------------------------------------------

        /// Fills m_expressions. Adding support for a new expression node is one line here.
        void registerExpressionHandlers();

        /// Type of an expression. `expected` is the type the context demands, when it has one; it is
        /// only used to pick the type of literals (enumeration/character literals, strings, aggregates).
        SemanticType exprType(const Expression& expr, const SemanticType* expected = nullptr);
        /// An identifier: an object (its declared type), a call of a parameterless function, or otherwise an enumeration
        /// literal or physical unit.
        SemanticType typeOfSymbol(const SymbolExpr& expr, const SemanticType* expected);
        /// `record.field`: the type of the field.
        SemanticType typeOfField(const FieldAccessExpr& expr, const SemanticType* expected);
        /// A node that is part of another construct (an association, a choice, a type mark ...) used as a value.
        template <typename Node>
        SemanticType typeOfNonValue(const Node& expr, const SemanticType* expected);
        [[noreturn]] void rejectNonValue(const Expression& expr) const;
        /// True for expressions whose type cannot be known without the context: character literals, strings, aggregates
        /// and bare enumeration literals.
        bool needsContext(const Expression& expr) const;
        /// True when the expression reads no signal, port, variable or loop parameter (required for initial values).
        bool isStaticExpression(const Expression& expr) const;
        /// The root object of a name (see RootObject); an empty view when the expression is not a name.
        RootObject rootObject(const Expression& name) const;
        /// A pure function may only read what it declares itself: reports an access to something declared outside.
        void checkObjectAccess(const RootObject& root, const ASTNode& at, bool write);

        // ---- Literals (expr_literals.cc) -------------------------------------------------------------

        /// Integer literal: universal integer.
        SemanticType typeOfInteger(const IntegerLiteralExpr& expr, const SemanticType* expected);
        /// Real literal: universal real.
        SemanticType typeOfReal(const DoubleLiteralExpr& expr, const SemanticType* expected);
        /// `10 ns`: the type owning the unit; the quantity must be a whole number of base units.
        SemanticType typeOfPhysical(const PhysicalLiteralExpr& expr, const SemanticType* expected);
        /// `'1'`: a value of an enumeration type chosen by context or uniqueness.
        SemanticType typeOfCharacter(const CharacterLiteralExpr& expr, const SemanticType* expected);
        /// String literal (bit strings are already expanded): needs an expected 1-D array whose element type contains every
        /// character; the length must equal the expected length when that is constrained.
        SemanticType typeOfString(const StringLiteralExpr& expr, const SemanticType* expected);
        /// Type of a non-object identifier or character literal: an overloaded enumeration literal resolved by context, else a
        /// physical unit. Explains ambiguity and unknown names in the message.
        SemanticType enumerationLiteralType(const std::string& literal, const SemanticType* expected, const ASTNode& node);

        // ---- Aggregates (aggregates.cc) --------------------------------------------------------------

        /// Aggregate `(a, b)` / `(0 => a, others => b)`: typed from the expected array or record type.
        SemanticType typeOfAggregate(const AggregateExpr& expr, const SemanticType* expected);
        /// Array aggregate: checks element types, index choices, and that every index is defined exactly once (or `others`).
        SemanticType arrayAggregate(const AggregateExpr& expr, const SemanticType& expected);
        /// Record aggregate: positional or named, each field assigned exactly once.
        SemanticType recordAggregate(const AggregateExpr& expr, const SemanticType& expected);

        // ---- Names: index, slice, conversion, qualified, external (expr_names.cc) --------------------

        /// `prefix(args)`: a type conversion when the prefix names a type, a function call when it names a function, else an
        /// index or slice of an array.
        SemanticType typeOfCall(const FunctionCallExpr& expr, const SemanticType* expected);
        /// Chooses between indexing and slicing by the shape of the single argument (a range means slice).
        SemanticType typeOfIndexOrSlice(const FunctionCallExpr& expr, const SemanticType& prefix);
        /// `arr(i)`: one index per dimension, each of the index type and (when static) inside the bounds; yields the element type.
        SemanticType typeOfIndex(const FunctionCallExpr& expr, const SemanticType& prefix);
        /// `arr(hi downto lo)`: the direction must match the array, static bounds must lie inside it; yields the array type with
        /// the slice bounds (unknown when dynamic).
        SemanticType typeOfSlice(const FunctionCallExpr& expr, const SemanticType& prefix);
        /// `T(x)`: legal between closely related types (numeric types, or arrays with the same element type); static lengths
        /// must agree. A literal operand is rejected because its type would be ambiguous.
        SemanticType typeOfConversion(const FunctionCallExpr& expr, const SemanticType& target);
        /// `T'(x)`: the operand must be a value of the type mark; it is typed by it, so literals and aggregates need no context.
        SemanticType typeOfQualified(const QualifiedExpr& expr, const SemanticType* expected);
        /// `<<signal .a.b : t>>`: a name whose object lives elsewhere in the hierarchy; it has the type written in it.
        SemanticType typeOfExternalName(const ExternalNameExpr& expr, const SemanticType* expected);
        /// When `index` is a constant, requires it to lie inside dimension `dimension` of `array`.
        void checkIndexInBounds(const Expression& index, const SemanticType& array, size_t dimension);
        /// True when `arg` is a range argument (a slice), not an index: `a to b`, `x'range`, a type mark or a subtype indication.
        bool isRangeArgument(const Expression& arg) const;

        // ---- Attributes (expr_attributes.cc, attributes.cc) ------------------------------------------

        /// Value attributes ('event 'length 'left 'right 'high 'low) and user-declared attributes. ('range is only valid
        /// where a range is expected.)
        SemanticType typeOfAttribute(const AttributeExpr& expr, const SemanticType* expected);
        /// The value of an attribute a design specified for the named item, or a report that it has none.
        SemanticType typeOfUserAttribute(const AttributeExpr& expr);
        /// The value a specification gave the item a plain name denotes for `attribute`, or nullptr.
        const AttributeValue* specifiedAttribute(const Expression& prefix, const std::string& attribute) const;
        /// True when `expr` is an identifier that names a type or subtype.
        bool isTypeName(const Expression& expr) const;
        /// Type an attribute applies to: the type itself for a type name, otherwise the type of the named object.
        SemanticType attributePrefixType(const Expression& prefix);
        /// True when `expr` is a signal or port, or an element/field/slice of one.
        bool namesSignal(const Expression& expr) const;

        /// `attribute name : type;`
        void declareAttribute(const AttributeDeclaration& decl);
        /// `attribute name of items : class is value;`
        void specifyAttribute(const AttributeSpecification& decl);
        /// Stores `value` as the attribute `name` of every item the specification names or covers.
        void applyAttributeSpecification(const AttributeSpecification& decl, const AttributeValue& value);
        /// Gives one named item its attribute value; the item must be declared in this region and of the specified class.
        void specifyItem(const AttributeSpecification& decl, const std::string& item, const AttributeValue& value, const ASTNode& at);
        /// Gives a newly declared label the values pending specifications of this region hold for it.
        void applyPendingLabelSpecs(const std::string& label);
        /// Ends the region: every pending specification must have found its label.
        void resolvePendingLabelSpecs();
        /// True when a symbol belongs to the class an attribute specification names.
        bool symbolHasClass(const Symbol& symbol, EntityClass entityClass) const;

        // ---- Operators (expr_operators.cc) -----------------------------------------------------------

        /// Unary operator: a design-declared function first, then the predefined rules; only `not` passes the expected type down.
        SemanticType typeOfUnary(const UnaryOpExpr& expr, const SemanticType* expected);
        /// Binary operator: a design-declared function first, then the predefined rules (see typeOperands); rejects a
        /// constant zero divisor.
        SemanticType typeOfBinary(const BinaryOpExpr& expr, const SemanticType* expected);
        /// Types both operands. Operands that need a context are typed after their partner, so `v = "0101"` takes the type of `v`
        /// and `a & '1' & "00"` takes the array type wanted by the surrounding assignment.
        OperandTypes typeOperands(const BinaryOpExpr& expr, const SemanticType* expected);
        /// The type a literal `operand` must have given the type of its partner `other` (invalid when it has none) and the
        /// expected type. Returns an invalid type when nothing decides it.
        SemanticType operandContext(const Expression& operand, const SemanticType& other, BinaryOperator op,
                                    const SemanticType* expected) const;
        /// A design-declared function named after the operator (`"+"`) applied to the operands, or nullopt when none is visible
        /// or none accepts them.
        std::optional<SemanticType> typeOfUserOperator(const Expression& node, const std::string& symbol,
                                                       const std::vector<const Expression*>& operands, const SemanticType* expected);

        // ---- Compatibility (compat.cc) ---------------------------------------------------------------

        /// Checks that a value of type `value` may be assigned to `target`; `what` names the context
        /// for the message (e.g. "signal 'y'"). Also range-checks static values.
        void checkAssignable(const SemanticType& target, const SemanticType& value, const Expression* valueExpr,
                             const ASTNode& at, const std::string& what);
        /// Why a value of type `value` cannot be assigned to `target` (empty when it can); looks at the types only.
        std::string assignmentProblem(const SemanticType& target, const SemanticType& value, const std::string& what) const;
        /// When `valueExpr` is a constant, requires it to lie inside the range of the scalar subtype `target`.
        void checkStaticRange(const SemanticType& target, const Expression& valueExpr, const std::string& what);
        /// A condition must be boolean; VHDL-2008 also allows std_logic (implicit `??` operator).
        void requireCondition(const Expression& condition, const std::string& what);
        /// `expr` must be a non-negative value of type time (`wait for` timeouts).
        void requireTime(const Expression& expr, const std::string& what);
        /// `type` must be an integer or enumeration type.
        /// LRM 9.3.6: numeric types convert to each other; arrays convert when dimensionality and element type match.
        bool closelyRelated(const SemanticType& from, const SemanticType& to) const;

        // ---- Design units and concurrent statements (analyzer.cc, statements.cc, instances.cc) ------

        /// Fills the design-unit, type-definition, declaration and statement tables. One line per supported node kind.
        void registerDispatchTables();
        /// Fills the sequential-statement table.
        void registerSequentialHandlers();
        /// Brings the context back to the library alone before a design unit: only the prelude is in scope and nothing an
        /// earlier unit left (even one whose analysis failed) is pending.
        void beginUnit();
        /// Resolves the generics and ports of an entity (duplicates are errors) and adds the entity to the library; the
        /// library must not have an entity of that name yet.
        void analyzeEntity(const EntityDeclaration& entity);
        /// Opens a scope with the generics and ports of the entity, analyzes the declarative part and every concurrent
        /// statement, then verifies the multiple-driver rule. Fails if the entity is not in the library or already has an
        /// architecture of that name.
        void analyzeArchitecture(const ArchitectureDeclaration& arch);
        /// Routes a statement of the architecture body to its handler and marks it as the current driver source.
        void analyzeConcurrentStatement(const Statement& statement);
        /// `with sel select[?] y <= v when choices, ...;`: target writable, values assignable, choices cover the selector.
        void analyzeWithClause(const WithClause& with);
        /// Instance of a declared component: the label is declared and the generic map and port map are checked.
        void analyzeComponentInstantiation(const ComponentInstantiation& instance);
        /// The generic map: each formal generic matched with a static actual of its type.
        void checkGenericMap(const ComponentInstantiation& instance, const Symbol& component);
        /// The port map: formals must exist and be connected once, inputs must all be connected, and each actual must fit its port.
        void checkPortMap(const ComponentInstantiation& instance, const Symbol& component);
        /// Checks one association: an input takes any expression of the port type; an output needs a writable name of that
        /// type (and becomes a driver); `open` is only legal for outputs and ports with a default.
        void connectPort(const Binding& binding, const FormalInfo& port, const ComponentInstantiation& instance);
        /// Declares a statement label in the current region; empty labels are ignored.
        void declareLabel(const std::string& label, const ASTNode& node);
        /// `target <= waveform;` in an architecture or a process: writable target, assignable value.
        void analyzeSignalAssignment(const SignalAssignment& assignment);
        /// A signal or variable assignment value: a plain expression typed against `target`, or a `v when c else ...` chain where
        /// every value is checked and every condition must be boolean. `unaffected` is only a signal assignment value.
        void analyzeValue(const Expression& value, const SemanticType& target, const ASTNode& at, const std::string& what,
                          bool allowUnaffected);
        /// Signal assignments may only drive signals and output ports: rejects input ports, constants, variables (use `:=`) and
        /// loop parameters with a message naming the problem.
        void checkWritable(const Expression& target, const std::string& what);
        /// `(a, b) <= value;` / `(a, b) := value;`: every element is a writable name of one type and the value an array of that type.
        void analyzeAggregateTarget(const AggregateExpr& target, const Expression& value, const ASTNode& at, bool signalTarget);
        /// `assert cond [report message] [severity level];`, in an architecture or a process.
        void analyzeAssert(const AssertStatement& statement);
        /// `proc(args);` in an architecture (a concurrent call) or in a process or subprogram (a sequential call).
        void analyzeProcedureCall(const ProcedureCallStatement& statement);

        // ---- Processes and sequential statements (sequential.cc, loops.cc) --------------------------

        /// Process: checks the sensitivity list names signals, then analyzes the local declarations and the body in a fresh scope.
        void analyzeProcess(const ProcessStatement& process);
        /// Analyzes a list of sequential statements in order.
        void analyzeSequence(const std::vector<StatementPtr>& statements);
        /// Routes one sequential statement to its handler.
        void analyzeSequentialStatement(const Statement& statement);
        /// `target := value;`: the root of the target must be a variable; the value must be assignable to it.
        void analyzeVariableAssignment(const VariableAssignment& assignment);
        /// if/elsif/else: every condition boolean, every branch analyzed.
        void analyzeIf(const IfStatement& statement);
        /// case[?]: the choices are checked against the selector (types, constants, duplicates, full coverage), then the bodies.
        void analyzeCase(const CaseStatement& statement);
        /// `wait [on signals] [until cond] [for time];` — illegal in a process with a sensitivity list and in a function.
        void analyzeWait(const WaitStatement& statement);
        /// `report message [severity level];`
        void analyzeReport(const ReportStatement& statement);
        /// `null;` has nothing to check.
        void analyzeNull(const NullStatement& statement);
        /// The message of assert/report: a value of the `string` type visible at that point.
        void analyzeMessage(const Expression& message, const ASTNode& at);
        /// The severity of assert/report must be a severity_level value; a null expression is accepted.
        void analyzeSeverity(const Expression* severity);
        /// `for i in range loop`: the parameter is a read-only constant local to the loop body.
        void analyzeForLoop(const ForLoopStatement& loop);
        /// `while cond loop`: the condition must be boolean.
        void analyzeWhileLoop(const WhileLoopStatement& loop);
        /// Unconditional `loop`.
        void analyzeLoop(const LoopStatement& loop);
        /// `exit [label] [when cond];` — see checkLoopControl.
        void analyzeExit(const ExitStatement& statement);
        /// `next [label] [when cond];` — see checkLoopControl.
        void analyzeNext(const NextStatement& statement);
        /// Registers a loop: its label must not repeat an enclosing loop's label.
        void enterLoop(const std::string& label, const ASTNode& node);
        /// exit/next must be inside a loop, a given label must name an enclosing loop, and the condition must be boolean.
        void checkLoopControl(const std::string& keyword, const std::string& label, const Expression* condition, const ASTNode& node);
        /// `return [value];` inside a subprogram: a function returns a value of its result type, a procedure none.
        void analyzeReturn(const ReturnStatement& statement);

        // ---- Choices (choices.cc) --------------------------------------------------------------------

        /// The list of choices behind a formal (`choices =>` of an aggregate, `when choices` of a case); fails when it is not
        /// one, or when `others` shares its list with other choices.
        const ChoiceListExpr& requireChoiceList(const Expression& formal);
        /// Checks the `when` choices of a case / selected assignment against the selector type:
        /// typing, constant values, no duplicates and full coverage (or `others`). `matching` selects `case?` / `select?`.
        void analyzeChoiceLists(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                const ASTNode& at, const std::string& what, bool matching);
        /// Integer / enumeration selectors: intervals of values must be disjoint, inside the selector's range and cover it.
        void analyzeDiscreteChoices(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                    const ASTNode& at, const std::string& what);
        /// Array-of-character selectors: every choice is a string literal of the selector's length, no repeats, and `others` is
        /// mandatory because listing every value is impractical.
        void analyzeArrayChoices(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                 const ASTNode& at, const std::string& what);
        /// Matching selectors (`case?`, `select?`): std_logic values or arrays of them, where '-' in a choice matches any value.
        void analyzeMatchingChoices(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                    const ASTNode& at, const std::string& what);

        // ---- Signal drivers (drivers.cc) -------------------------------------------------------------

        /// Attributes the signal assignments analyzed next to `statement` (a process, instance or concurrent assignment).
        void beginDriverSource(const Statement& statement);
        /// Records that the current source drives the signal `target` is built on (once per source).
        void recordDriver(const Expression& target, const ASTNode& at);
        /// A signal of an unresolved type (integer, enumerations, records of them, ...) may have only one whole-signal source.
        /// Element/slice drivers only conflict with a whole-signal driver, because two sources may legally drive different
        /// elements of an array. Called at the end of each architecture.
        void checkDrivers();

        // ---- Subprograms (subprograms.cc) ------------------------------------------------------------

        /// `function f(...) return t;` / `procedure p(...);`: registers the profile so calls can resolve to it.
        void declareSubprogram(const SubprogramDeclaration& decl);
        /// A subprogram body: completes a declaration or declares the subprogram, then analyzes the body in its own scope.
        void declareSubprogramBody(const SubprogramBody& decl);
        /// Resolves the profile of a specification (parameter classes, modes, types, result) without declaring anything.
        SubprogramInfo resolveSubprogramSpec(const SubprogramSpec& spec);
        /// Adds the subprogram to the overloads visible under its name (a homograph is an error) and returns the entry.
        SubprogramInfo& registerSubprogram(SubprogramInfo profile, const ASTNode& at, bool isBody);
        /// Operators must have the right number of parameters for what they overload.
        void checkOperatorProfile(const SubprogramInfo& profile, const ASTNode& at) const;
        /// Analyzes the body of `info` in a new scope holding the parameters of `profile` (the body's own spelling of them).
        void analyzeSubprogramBody(const SubprogramBody& decl, SubprogramInfo& info, const SubprogramInfo& profile);
        /// Requires that every subprogram declared without a body in the current region got one.
        void checkBodiesDefined();
        /// True when the profiles have the same parameter and result base types.
        static bool sameProfile(const SubprogramInfo& a, const SubprogramInfo& b);
        /// The overload whose profile a signature (`[t1, t2 return t3]`) spells out. Fails when none or several match.
        const SubprogramInfo& selectBySignature(const std::vector<const SubprogramInfo*>& overloads, const SignatureExpr& signature,
                                                const ASTNode& at);

        // ---- Calls and overload resolution (calls.cc) ------------------------------------------------

        /// A call of the function or procedure overloads visible under `name` with the given associations. Picks the overload
        /// the actuals (and, for functions, the expected result type) select, checks the actuals and records the call.
        const SubprogramInfo& resolveCall(const std::string& name, const ASTNode& node, const std::vector<const Expression*>& arguments,
                                          const SemanticType* expected, bool wantFunction);
        /// Why `candidate` cannot be called with `arguments` (empty when it can); nothing is thrown.
        std::string candidateProblem(const SubprogramInfo& candidate, const std::vector<const Expression*>& arguments, const ASTNode& at);
        /// Checks the actuals of the chosen overload and records the call (callee graph, drivers of signal actuals).
        void commitCall(const SubprogramInfo& callee, const ASTNode& node, const std::vector<const Expression*>& arguments);
        /// Type of a call of a function.
        SemanticType typeOfFunctionCall(const std::string& name, const ASTNode& node, const std::vector<const Expression*>& arguments,
                                        const SemanticType* expected);
        /// Text listing the overloads under `name`, for diagnostics.
        std::string describeOverloads(const std::vector<const SubprogramInfo*>& overloads) const;
        /// `f(int, bit) return bit` for diagnostics.
        std::string describeProfile(const SubprogramInfo& info) const;

        // ---- Purity and body rules (purity.cc) -------------------------------------------------------

        /// True when every path through `statements` ends in a `return` (or never ends).
        bool alwaysReturns(const std::vector<StatementPtr>& statements) const;
        /// True when `statements` contain an exit that leaves the loop labeled `label` (or an unlabeled one when `direct`).
        bool exitsLoop(const std::vector<StatementPtr>& statements, const std::string& label, bool direct) const;
        /// Body rules that need every body of the region: a function cannot reach a wait, and a process with a sensitivity
        /// list cannot call a procedure that waits.
        void checkCallGraph(size_t firstSubprogram);
        /// Notes that the code being analyzed calls `callee` (for checkCallGraph).
        void noteCall(const SubprogramInfo& callee, const ASTNode& at);

        // ---- Aliases (aliases.cc) --------------------------------------------------------------------

        /// `alias name [: subtype] is target [signature];` for objects, types, subprograms and enumeration literals.
        void declareAlias(const AliasDeclaration& decl);
        void declareObjectAlias(const AliasDeclaration& decl);
        void declareSubprogramAlias(const AliasDeclaration& decl);
    };

    template <typename Node>
    void AnalyzerContext::misplacedDeclaration(const Node& decl)
    {
        fail("This declaration can only appear in the interface list it belongs to", decl);
    }

    template <typename Node>
    SemanticType AnalyzerContext::typeOfNonValue(const Node& expr, const SemanticType*)
    {
        rejectNonValue(expr);
    }

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_ANALYZER_INTERNAL_H
