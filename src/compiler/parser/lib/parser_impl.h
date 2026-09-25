#ifndef PULSE_PARSER_LIB_PARSER_IMPL_H
#define PULSE_PARSER_LIB_PARSER_IMPL_H

#include "token_stream.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Pulse::Parser
{
    /// The declarative regions. Each allows its own set of declarations (the grammar, not a scope rule).
    enum class Region { Architecture, Process, Subprogram };

    /// Recursive-descent parser for one file: one member function per grammar rule.
    ///
    /// Conventions every rule follows:
    ///  * A rule is called with the cursor on its first token and returns with the cursor just past its last one.
    ///  * A rule either consumes at least one token or throws, so no loop can spin in place.
    ///  * Nested rules (expressions, statements, element resolutions, subprogram bodies) hold a NestingGuard, so
    ///    hostile input fails with a syntax error instead of overflowing the stack.
    class Parser
    {
    public:
        explicit Parser(Tokenizer& tokenizer);

        ASTRoot parseFile();                                        ///< Every design unit up to the end of the file.
        std::vector<DeclarationPtr> parseStandaloneDeclarations();  ///< A declarative part up to the end of the file.

    private:
        TokenStream tokens;
        int depth = 0;
        size_t treeDepth = 0;   ///< Length of the chains being built along the path from the root of the tree to the token being parsed

        // ---- Shared helpers (parser.cc) ---------------------------------------------------------

        /// Limits nesting (parentheses, statements ...) so deep input cannot overflow the stack.
        class NestingGuard
        {
            Parser& m_parser;
        public:
            explicit NestingGuard(Parser& parser);
            ~NestingGuard();
            NestingGuard(const NestingGuard&) = delete;
            NestingGuard& operator=(const NestingGuard&) = delete;
        };

        /// Limits how long the chains of one expression can get (`a + b + c + ...`, `a.b.c.d`, `x when c else y when d else ...`). The
        /// parser builds a chain in a loop, so its length is unbounded, but the tree it makes is that deep, and everything that
        /// walks a tree recurses along it: cloning, printing, the analyzer. The limit keeps every one of them within the stack.
        class ChainScope
        {
            Parser& m_parser;
            size_t m_links = 0;
        public:
            explicit ChainScope(Parser& parser) : m_parser(parser) { }
            ~ChainScope() { m_parser.treeDepth -= m_links; }
            ChainScope(const ChainScope&) = delete;
            ChainScope& operator=(const ChainScope&) = delete;

            /// One more link in the chain; fails when the expression has become too deep.
            void extend();
        };

        /// A new node located at `start`.
        template <typename T>
        std::unique_ptr<T> node(const Token* start)
        {
            auto created = std::make_unique<T>();
            created->source = locationOf(start);
            return created;
        }

        /// One declaration per name of `names` (`signal a, b : bit;` declares a and b), each filled by `fill`.
        template <typename T, typename Fill>
        std::vector<std::unique_ptr<T>> declareEach(const std::vector<const Token*>& names, Fill fill)
        {
            std::vector<std::unique_ptr<T>> declarations;
            for (const Token* name : names)
            {
                auto declaration = node<T>(name);
                declaration->name = name->value;
                fill(*declaration);
                declarations.push_back(std::move(declaration));
            }
            return declarations;
        }

        /// Calls `parseOne` for each item of `item {separator item}`.
        template <typename Fn>
        void parseSeparated(std::string_view separator, Fn parseOne)
        {
            do parseOne();
            while (tokens.accept(separator));
        }

        /// Calls `parseOne` for each item of `( item {, item} )`.
        template <typename Fn>
        void parseParenthesized(Fn parseOne)
        {
            tokens.expect("(");
            parseSeparated(",", parseOne);
            tokens.expect(")");
        }

        std::string parseOptionalLabel();                           ///< `identifier :` in front of a statement, or "".
        void expectEnd(std::string_view kind, const std::string& name);         ///< `end kind [name] ;`
        void expectEndOptionalKind(std::string_view kind, const std::string& name); ///< `end [kind] [name] ;`
        void finishEnd(const std::string& name);                    ///< `[name] ;` after `end kind`.
        void acceptClosingName(const std::string& name);            ///< An optional closing name, which must match.

        // ---- Design units (design_units.cc) -----------------------------------------------------

        DesignUnitPtr parseDesignUnit();
        DesignUnitPtr parseLibraryUnit();
        std::vector<DeclarationPtr> parseContextClause();
        std::unique_ptr<LibraryClause> parseLibraryClause();
        std::unique_ptr<UseClause> parseUseClause();
        ExpressionPtr parseSelectedName();                          ///< `a.b.c` or `a.b.all` (use clauses).
        std::unique_ptr<EntityDeclaration> parseEntity();
        std::unique_ptr<ArchitectureDeclaration> parseArchitecture();

        // ---- Interfaces (interfaces.cc) ---------------------------------------------------------

        std::vector<std::unique_ptr<GenericDeclaration>> parseOptionalGenericClause();
        std::vector<std::unique_ptr<PortDeclaration>> parseOptionalPortClause();
        std::vector<std::unique_ptr<GenericDeclaration>> parseGenericGroup();
        std::vector<std::unique_ptr<PortDeclaration>> parsePortGroup();
        std::vector<std::unique_ptr<ParameterDeclaration>> parseParameterList();
        std::vector<std::unique_ptr<ParameterDeclaration>> parseParameterGroup();
        ParameterClass parseParameterClass();
        PortMode parseOptionalMode();
        std::vector<const Token*> parseIdentifierList();            ///< `a, b, c`
        ExpressionPtr parseOptionalDefault();                       ///< `:= expression`, or null.
        std::vector<ExpressionPtr> parseOptionalMap(std::string_view kind); ///< `generic map (...)` / `port map (...)`
        ExpressionPtr parseAssociation();                           ///< `[formal =>] actual`
        ExpressionPtr parseActual();                                ///< `open`, a range or an expression.

        // ---- Declarations (declarations.cc) -----------------------------------------------------

        std::vector<DeclarationPtr> parseDeclarativePart(Region region);
        bool atDeclaration(Region region) const;
        void parseDeclaration(std::vector<DeclarationPtr>& out);
        std::vector<std::unique_ptr<SignalDeclaration>> parseSignalDeclaration();
        std::vector<std::unique_ptr<ConstantDeclaration>> parseConstantDeclaration();
        std::vector<std::unique_ptr<VariableDeclaration>> parseVariableDeclaration();
        std::unique_ptr<ComponentDeclaration> parseComponentDeclaration();
        std::unique_ptr<AliasDeclaration> parseAliasDeclaration();
        std::string parseDesignator();                              ///< Identifier, character literal or operator symbol.
        DeclarationPtr parseAttribute();                            ///< Declaration or specification.
        std::unique_ptr<AttributeDeclaration> finishAttributeDeclaration(const Token* start, const Token* name);
        std::unique_ptr<AttributeSpecification> finishAttributeSpecification(const Token* start, const Token* name);
        std::vector<ExpressionPtr> parseEntityNameList();
        EntityClass parseEntityClass();

        // ---- Types (types.cc) -------------------------------------------------------------------

        std::unique_ptr<TypeDeclaration> parseTypeDeclaration();
        std::unique_ptr<SubtypeDeclaration> parseSubtypeDeclaration();
        TypeDefinitionPtr parseTypeDefinition(const std::string& typeName);
        std::unique_ptr<EnumeratedTypeDefinition> parseEnumeration();
        std::string parseEnumerationLiteral();
        TypeDefinitionPtr parseRangeTypeDefinition(const std::string& typeName);
        std::unique_ptr<PhysicalTypeDefinition> finishPhysicalType(const Token* start, ExpressionPtr range, const std::string& typeName);
        std::unique_ptr<UnitDeclaration> parseBaseUnit();           ///< `name ;`
        std::unique_ptr<UnitDeclaration> parseSecondaryUnit();      ///< `name = [literal] unit ;`
        TypeDefinitionPtr parseArrayType();
        bool atUnconstrainedIndex() const;
        std::unique_ptr<UnconstrainedArrayTypeDefinition> finishUnconstrainedArray(const Token* start);
        std::unique_ptr<ArrayTypeDefinition> finishConstrainedArray(const Token* start);
        std::string parseUnconstrainedIndex();                      ///< `type_mark range <>`
        std::unique_ptr<TypeSpec> parseArrayElementType();          ///< `of subtype_indication`
        std::unique_ptr<RecordTypeDefinition> parseRecord(const std::string& typeName);
        std::vector<std::unique_ptr<RecordField>> parseRecordFieldGroup();

        std::unique_ptr<TypeSpec> parseTypeSpec();                  ///< Subtype indication.
        std::unique_ptr<TypeSpec> parseTypeMark();                  ///< A bare type mark, as a TypeSpec.
        ExpressionPtr parseOptionalResolution();
        ExpressionPtr parseElementResolution();
        void parseOptionalConstraint(TypeSpec& spec);
        std::vector<ExpressionPtr> parseIndexConstraint();
        ExpressionPtr parseRangeConstraint();                       ///< `range <range>`

        // ---- Subprograms (subprograms.cc) -------------------------------------------------------

        DeclarationPtr parseSubprogram();
        std::unique_ptr<SubprogramSpec> parseSubprogramSpec();
        std::unique_ptr<SubprogramSpec> parseFunctionSpec(const Token* start, bool impure);
        std::unique_ptr<SubprogramSpec> parseProcedureSpec();
        std::string parseSubprogramName();
        std::vector<std::unique_ptr<ParameterDeclaration>> parseOptionalParameters();
        std::unique_ptr<SubprogramBody> parseSubprogramBody(std::unique_ptr<SubprogramSpec> spec);

        // ---- Concurrent statements (concurrent_statements.cc) -----------------------------------

        std::vector<StatementPtr> parseConcurrentStatements();      ///< Up to `end`.
        StatementPtr parseConcurrentStatement();
        StatementPtr parseUnlabeledConcurrentStatement(const std::string& label);
        bool atInstantiation() const;
        std::unique_ptr<ProcessStatement> parseProcess(const std::string& label);
        void parseSensitivityList(ProcessStatement& process);
        std::unique_ptr<ComponentInstantiation> parseInstantiation();

        // ---- Sequential statements (sequential_statements.cc) -----------------------------------

        std::vector<StatementPtr> parseSequentialStatements(std::initializer_list<std::string_view> stops);
        StatementPtr parseSequentialStatement();
        StatementPtr parseUnlabeledSequentialStatement(const std::string& label);
        std::unique_ptr<IfStatement> parseIf(const std::string& label);
        std::unique_ptr<IfBranch> parseIfBranch(std::string_view keyword);
        std::unique_ptr<CaseStatement> parseCase(const std::string& label);
        std::unique_ptr<CaseAlternative> parseCaseAlternative();
        void expectEndCase(bool matching, const std::string& label);
        std::unique_ptr<ForLoopStatement> parseForLoop(const std::string& label);
        std::unique_ptr<WhileLoopStatement> parseWhileLoop(const std::string& label);
        std::unique_ptr<LoopStatement> parsePlainLoop(const std::string& label);
        std::vector<StatementPtr> parseLoopBody(const std::string& label);  ///< `loop statements end loop [label];`
        StatementPtr parseExit();
        StatementPtr parseNext();
        std::string parseOptionalLoopLabel();
        StatementPtr parseNull();
        StatementPtr parseWait();
        std::vector<ExpressionPtr> parseOptionalOnList();
        ExpressionPtr parseOptionalClause(std::string_view keyword);   ///< `keyword expression`, or null.
        StatementPtr parseReport();
        StatementPtr parseReturn();

        // ---- Assignments and calls (assignments.cc) ---------------------------------------------

        StatementPtr parseAssert();
        StatementPtr parseAssignmentOrCall(bool sequential);
        ExpressionPtr parseTarget();
        StatementPtr finishSignalAssignment(ExpressionPtr target);
        StatementPtr finishVariableAssignment(ExpressionPtr target);
        StatementPtr finishProcedureCall(ExpressionPtr call);
        std::unique_ptr<WithClause> parseSelectedAssignment();
        std::unique_ptr<SelectedChoice> parseSelectedChoice();
        ExpressionPtr parseConditionalValue();                      ///< `value [when c else value ...]`
        std::unique_ptr<WhenElseExpr> startWhenElse(ExpressionPtr value);
        ExpressionPtr parseValue();                                 ///< An expression or `unaffected`.

        // ---- Expressions (expressions.cc) -------------------------------------------------------

        ExpressionPtr parseExpression();
        ExpressionPtr parseCondition();                             ///< `?? primary`
        ExpressionPtr parseLogical();
        ExpressionPtr parseLogicalChain(ExpressionPtr left, BinaryOperator op);
        ExpressionPtr parseRelation();
        ExpressionPtr parseShift();
        ExpressionPtr parseSimpleExpression();
        ExpressionPtr parseSignedTerm();
        ExpressionPtr parseTerm();
        ExpressionPtr parseFactor();
        ExpressionPtr parseUnaryFactor(UnaryOperator op);
        ExpressionPtr parsePrimary();

        // ---- Names (names.cc) -------------------------------------------------------------------

        ExpressionPtr parseName();
        ExpressionPtr parseNamePrefix();                            ///< An identifier, an operator symbol or an external name.
        ExpressionPtr parseSymbol();                                ///< A plain identifier.
        ExpressionPtr parseOperatorSymbol();                        ///< `"and"` as a name.
        ExpressionPtr parseNameSuffixes(ExpressionPtr name);
        ExpressionPtr parseSelectedSuffix(ExpressionPtr prefix);
        ExpressionPtr parseCallSuffix(ExpressionPtr callee);
        ExpressionPtr parseTickSuffix(ExpressionPtr prefix, std::unique_ptr<SignatureExpr> signature);
        ExpressionPtr parseQualifiedSuffix(ExpressionPtr prefix);
        std::string parseAttributeDesignator();
        std::unique_ptr<SignatureExpr> parseSignature();
        ExpressionPtr parseExternalName();
        ExternalObjectClass parseExternalObjectClass();
        std::string parseExternalPath();

        // ---- Choices, ranges and aggregates (choices.cc) ----------------------------------------

        ExpressionPtr parseChoices();                               ///< `choice {| choice}`
        ExpressionPtr parseChoicesFrom(ExpressionPtr first);
        ExpressionPtr parseChoice();
        ExpressionPtr parseDiscreteRange();                         ///< Range, `type range r`, or expression.
        ExpressionPtr parseRangeOrExpression();
        ExpressionPtr parseRange();                                 ///< `a to b`, `a downto b` or a range attribute.
        ExpressionPtr finishRange(ExpressionPtr low);
        ExpressionPtr parseAggregateOrParenthesized();
        ExpressionPtr parseAggregateElement();
        ExpressionPtr finishNamedElement(ExpressionPtr choices);

        // ---- Literals (literals.cc) -------------------------------------------------------------

        ExpressionPtr parseNumericLiteral();                        ///< Integer, real or physical literal.
        ExpressionPtr parseAbstractLiteral();                       ///< Integer or real.
        ExpressionPtr parseCharacterLiteral();
        ExpressionPtr parseStringLiteral();
    };

    // ---- Literal values (literals.cc, bit_strings.cc) -------------------------------------------

    bool isRealLiteral(const Token& token);             ///< The numeric token has a point (`1.5`, `2#1.1#`).
    int64_t integerValue(const Token& token);           ///< Value of an integer literal; throws ast_syntax_error on overflow.
    double realValue(const Token& token);               ///< Value of a real literal; throws ast_syntax_error if not finite.
    /// Characters of a string literal: doubled quotes un-escaped, bit strings expanded by their prefix.
    std::string stringValue(const Token& token);
    /// An operator symbol used as a name (`"AND"`): its lowercased text with the quotes kept (`"and"`).
    std::string operatorSymbolName(const Token& token);

    /// Moves every element of `items` to the end of `out` (e.g. typed declarations into a DeclarationPtr list).
    template <typename Base, typename Derived>
    void appendAll(std::vector<std::unique_ptr<Base>>& out, std::vector<std::unique_ptr<Derived>> items)
    {
        for (auto& item : items)
            out.push_back(std::move(item));
    }

} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_PARSER_IMPL_H
