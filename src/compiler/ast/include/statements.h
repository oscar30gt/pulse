#ifndef PULSE_VHDL_AST_STATEMENTS_H
#define PULSE_VHDL_AST_STATEMENTS_H

#include "node.h"

namespace Pulse::Parser
{
    // The label of a statement (`lbl : ...`) is Statement::label. It is the instance name of a
    // component instantiation and the name of a process or loop.

    // --------------------------------------------------------------------------------------------
    // Concurrent statements (a signal assignment may also appear in a process)
    // --------------------------------------------------------------------------------------------

    /// Signal assignment: "target <= value;". A conditional assignment has a WhenElseExpr as its value.
    struct SignalAssignment final : Statement
    {
        ExpressionPtr target;   /// Assigned name (signal, slice, field, ...)
        ExpressionPtr value;

        PULSE_AST_NODE_OVERRIDES
    };

    /// One `value when choices` alternative of a selected signal assignment.
    struct SelectedChoice final : ASTNode
    {
        ExpressionPtr value;    /// Value assigned when the choices match
        ExpressionPtr choices;  /// A ChoiceListExpr (possibly `others`)

        PULSE_AST_NODE_OVERRIDES
    };

    /// Selected signal assignment: "with selector select[?] target <= value when choices, ...;".
    struct WithClause final : Statement
    {
        bool matching = false;  /// `select?` (VHDL-2008 matching selection)
        ExpressionPtr selector;
        ExpressionPtr target;
        std::vector<std::unique_ptr<SelectedChoice>> choices;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Instantiation of a component: "label : [component] comp [generic map (...)] [port map (...)];".
    /// The label is the instance name.
    struct ComponentInstantiation final : Statement
    {
        std::string componentName;
        /// Associations in source order: NamedAssociationExpr (`open` is an OpenExpr actual) or a positional expression.
        std::vector<ExpressionPtr> genericMap;
        std::vector<ExpressionPtr> portMap;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Process statement: "[label :] process [(sensitivity)] [is] <declarations> begin <statements> end process;".
    struct ProcessStatement final : Statement
    {
        bool sensitivityAll = false;                /// `process(all)`
        std::vector<ExpressionPtr> sensitivityList; /// Sensitivity list names (empty when there is none or `all`)
        std::vector<DeclarationPtr> declarations;   /// Declarative part, in source order
        std::vector<StatementPtr> body;             /// Sequential statements

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Sequential statements
    // --------------------------------------------------------------------------------------------

    /// Variable assignment: "target := value;".
    struct VariableAssignment final : Statement
    {
        ExpressionPtr target;
        ExpressionPtr value;

        PULSE_AST_NODE_OVERRIDES
    };

    /// An `if` or `elsif` branch.
    struct IfBranch final : ASTNode
    {
        ExpressionPtr condition;            /// Condition of this branch
        std::vector<StatementPtr> body;     /// Statements executed when the condition is true

        PULSE_AST_NODE_OVERRIDES
    };

    /// if / elsif / else statement.
    struct IfStatement final : Statement
    {
        std::vector<std::unique_ptr<IfBranch>> branches;    /// `if` + zero or more `elsif` branches, in order
        std::vector<StatementPtr> elseBody;                 /// Statements of the else clause (may be empty)

        PULSE_AST_NODE_OVERRIDES
    };

    /// One `when choices => statements` alternative of a case statement.
    struct CaseAlternative final : ASTNode
    {
        ExpressionPtr choices;              /// A ChoiceListExpr (possibly `others`)
        std::vector<StatementPtr> body;

        PULSE_AST_NODE_OVERRIDES
    };

    /// case statement: "case[?] selector is when choices => statements ... end case[?];".
    struct CaseStatement final : Statement
    {
        bool matching = false;  /// `case?` (VHDL-2008 matching case)
        ExpressionPtr selector;
        std::vector<std::unique_ptr<CaseAlternative>> alternatives;

        PULSE_AST_NODE_OVERRIDES
    };

    /// "for <parameter> in <range> loop ... end loop;". `range` is a `To`/`Downto` BinaryOpExpr,
    /// or a name denoting a discrete type or range (e.g. a type mark or `arr'range`).
    struct ForLoopStatement final : Statement
    {
        std::string parameter;
        ExpressionPtr range;
        std::vector<StatementPtr> body;

        PULSE_AST_NODE_OVERRIDES
    };

    /// "while <condition> loop ... end loop;".
    struct WhileLoopStatement final : Statement
    {
        ExpressionPtr condition;
        std::vector<StatementPtr> body;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Unconditional "loop ... end loop;".
    struct LoopStatement final : Statement
    {
        std::vector<StatementPtr> body;

        PULSE_AST_NODE_OVERRIDES
    };

    /// "exit [loop_label] [when condition];"
    struct ExitStatement final : Statement
    {
        std::string loopLabel;      /// Label of the loop to leave, empty for the innermost one
        ExpressionPtr condition;    /// The `when` condition, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// "next [loop_label] [when condition];"
    struct NextStatement final : Statement
    {
        std::string loopLabel;      /// Label of the loop to continue, empty for the innermost one
        ExpressionPtr condition;    /// The `when` condition, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// "null;"
    struct NullStatement final : Statement
    {
        PULSE_AST_NODE_OVERRIDES
    };

    /// "wait [on names] [until condition] [for timeout];"  ("wait;" waits forever).
    struct WaitStatement final : Statement
    {
        std::vector<ExpressionPtr> onSignals;
        ExpressionPtr until;        /// The `until` condition, or null
        ExpressionPtr timeout;      /// The `for` time expression, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// "assert condition [report message] [severity level];"
    struct AssertStatement final : Statement
    {
        ExpressionPtr condition;
        ExpressionPtr message;      /// The report expression, or null
        ExpressionPtr severity;     /// Severity expression, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// "report message [severity level];"
    struct ReportStatement final : Statement
    {
        ExpressionPtr message;
        ExpressionPtr severity;     /// Severity expression, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// "return [value];" inside a subprogram body.
    struct ReturnStatement final : Statement
    {
        ExpressionPtr value;        /// The returned expression, or null in a procedure

        PULSE_AST_NODE_OVERRIDES
    };

    /// Procedure call, sequential or concurrent: "proc;" or "proc(a, b => c);".
    struct ProcedureCallStatement final : Statement
    {
        ExpressionPtr call;         /// A SymbolExpr (no arguments) or a FunctionCallExpr

        PULSE_AST_NODE_OVERRIDES
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_STATEMENTS_H
