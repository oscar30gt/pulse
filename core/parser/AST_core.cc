#include "AST.h"

#include <cctype>
#include <cstring>
#include <deque>
#include <algorithm>

namespace Pulse::Parser::VHDL
{
    namespace
    {
        // ===================================================================
        // Utilities
        // ===================================================================

        /// @brief Parses a VHDL integer literal string into a 64-bit value.
        /// @details Underscore separators (e.g. 1_000) are ignored per the VHDL standard.
        /// @param raw The raw token text, possibly containing underscores.
        /// @return The numeric value of the literal.
        int64_t parseIntegerLiteralText(const std::string& raw)
        {
            std::string digits;
            digits.reserve(raw.size());
            for (char c : raw)
                if (c != '_') digits.push_back(c);
            return std::stoll(digits);
        }

        // ===================================================================
        // TokenStream
        // ===================================================================

        /// @brief Wraps a @ref Tokenizer with look-ahead buffering.
        /// @details Tokens are pulled from the tokenizer on demand and cached in a
        ///          deque so callers can peek arbitrarily far ahead without consuming
        ///          tokens prematurely.
        class TokenStream
        {
        public:
            explicit TokenStream(Tokenizer& src) : m_src(src) { }

            /// @brief Returns the token @p n positions ahead without consuming it.
            /// @param n Zero-based look-ahead offset (0 = next token).
            /// @return A reference to the peeked token, or a static EOF sentinel.
            const Token& peek(size_t n = 0)
            {
                fill(n);
                if (n < m_buffer.size()) return m_buffer[n];
                static const Token eofToken{ TokenType::Unknown, "", 0, 0 };
                return eofToken;
            }

            /// @brief Consumes and returns the next token.
            /// @throws std::runtime_error if the stream has ended.
            Token next()
            {
                fill(0);
                if (m_buffer.empty())
                    throw std::runtime_error("VHDL parse error: unexpected end of input");
                Token t = std::move(m_buffer.front());
                m_buffer.pop_front();
                return t;
            }

            /// @brief Returns true when there are no more tokens to consume.
            bool eof()
            {
                fill(0);
                return m_buffer.empty() && m_ended;
            }

        private:
            /// @brief Ensures at least @p n+1 tokens are in the buffer, pulling from the
            ///        tokenizer until enough tokens are available or the source is exhausted.
            void fill(size_t n)
            {
                while (m_buffer.size() <= n && !m_ended)
                {
                    Token t;
                    m_src >> t;
                    if (t.value.empty())
                    {
                        m_ended = true;
                        break;
                    }
                    m_buffer.push_back(std::move(t));
                }
            }

            Tokenizer& m_src;
            std::deque<Token> m_buffer;
            bool m_ended = false;
        };

        // ===================================================================
        // SignalInfo / TypeSpec
        // ===================================================================

        /// @brief Internal bookkeeping for a declared signal or port.
        /// @details Never appears in the AST. Used exclusively to normalise and
        ///          flatten bit-select and range-select indices so that internal
        ///          position 0 always refers to the rightmost bit of the declaration,
        ///          regardless of whether the signal was declared with DOWNTO or TO.
        struct SignalInfo
        {
            Pulse::bitWidth_t width = 1;
            bool isDownto = true;     ///< True when declared as (high DOWNTO low).
            int64_t declaredLow = 0;  ///< min(bound0, bound1) of the declaration.
            int64_t declaredHigh = 0; ///< max(bound0, bound1) of the declaration.
            bool isVector = false;    ///< False for plain STD_LOGIC scalars.
        };

        /// @brief Intermediate result of parsing a VHDL type specification.
        /// @details Used to carry type info from parseTypeSpec to its callers
        ///          before the relevant @ref SignalInfo or @ref PortDeclaration is created.
        struct TypeSpec
        {
            Pulse::bitWidth_t width;
            bool isVector;
            bool isDownto;
            int64_t low;
            int64_t high;
        };

        // ===================================================================
        // Parse Context & Forward Declarations
        // ===================================================================

        /// @brief Holds all transient state required during parsing.
        struct ParseContext
        {
            TokenStream stream;

            /// @brief All signals and ports visible in the current architecture scope.
            std::unordered_map<std::string, SignalInfo> symbols;

            /// @brief Port metadata for every entity seen so far, keyed by entity name.
            std::unordered_map<std::string, std::unordered_map<std::string, SignalInfo>> entityPortInfo;

            explicit ParseContext(Tokenizer& tokenizer) : stream(tokenizer) {}
        };

        const Token& peek(ParseContext& ctx, size_t n = 0);
        Token next(ParseContext& ctx);
        Token expectValue(ParseContext& ctx, const char* v);
        Token expectIdentifier(ParseContext& ctx);
        [[noreturn]] void error(ParseContext& ctx, const std::string& msg);
        void skipToSemicolon(ParseContext& ctx);
        void parseEndClause(ParseContext& ctx);
        bool matchesAny(const std::string& v, const char* const* opts, size_t n);

        std::unique_ptr<EntityDeclaration> parseEntity(ParseContext& ctx);
        std::unique_ptr<ComponentDeclaration> parseComponentDeclaration(ParseContext& ctx);
        std::unique_ptr<ArchitectureDeclaration> parseArchitecture(ParseContext& ctx);
        void parsePortList(ParseContext& ctx, std::vector<std::unique_ptr<PortDeclaration>>& ports, std::unordered_map<std::string, SignalInfo>& infoOut);
        TypeSpec parseTypeSpec(ParseContext& ctx);
        std::vector<std::unique_ptr<SignalDeclaration>> parseSignalDeclarations(ParseContext& ctx);
        std::unique_ptr<ComponentInstantiation> parseInstantiation(ParseContext& ctx);
        std::unique_ptr<SignalAssignment> parseSignalAssignment(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseAssignmentValue(ParseContext& ctx);

        std::unique_ptr<ProcessStatement> parseProcess(ParseContext& ctx, const std::string& label);
        std::unique_ptr<SequentialStatement> parseSequentialStatement(ParseContext& ctx);
        std::unique_ptr<IfStatement> parseIfStatement(ParseContext& ctx);
        std::unique_ptr<WaitForStatement> parseWaitFor(ParseContext& ctx);
        std::unique_ptr<WaitForeverStatement> parseWaitForever(ParseContext& ctx);
        std::vector<std::unique_ptr<SequentialStatement>> parseSequentialBody(ParseContext& ctx);

        std::unique_ptr<IntegerLiteralExpr> makeIntLit(int64_t v);
        int64_t flattenIndex(const SignalInfo& info, int64_t i);
        std::unique_ptr<SignalReference> parseSignalReference(ParseContext& ctx);

        int64_t parseConstIntExpr(ParseContext& ctx);
        int64_t parseConstMul(ParseContext& ctx);
        int64_t parseConstFactor(ParseContext& ctx);
        int64_t evalConstAttribute(ParseContext& ctx, const std::string& signalName, std::string attr);

        std::unique_ptr<ASTNode> parseIntegerExpr(ParseContext& ctx);
        std::unique_ptr<ASTNode> parseIntMul(ParseContext& ctx);
        std::unique_ptr<ASTNode> parseIntFactor(ParseContext& ctx);
        AttributeKind attributeKindFromString(std::string s);

        std::unique_ptr<Expression<ReturnType::LOGIC>> parseValueExpr(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseLogicalLevel(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseShiftLevel(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseAddingLevel(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseUnaryLevel(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseMultLevel(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> parsePrimary(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::LOGIC>> wrapBinary(const std::string& op, std::unique_ptr<Expression<ReturnType::LOGIC>> left, std::unique_ptr<Expression<ReturnType::LOGIC>> right);

        std::unique_ptr<Expression<ReturnType::BOOLEAN>> parseCondition(ParseContext& ctx);
        std::unique_ptr<Expression<ReturnType::BOOLEAN>> parseBoolTerm(ParseContext& ctx);

        std::unique_ptr<LogicLiteralExpr> parseLogicLiteralToken(ParseContext& ctx);
        std::unique_ptr<LogicLiteralExpr> bitStringToLiteral(ParseContext& ctx, const std::string& raw);
        std::unique_ptr<LogicLiteralExpr> charToLiteral(ParseContext& ctx, const std::string& raw);

        // ===============================================================
        // Token helpers
        // ===============================================================

        /// @brief Returns the token @p n positions ahead without consuming it.
        const Token& peek(ParseContext& ctx, size_t n) { return ctx.stream.peek(n); }

        /// @brief Consumes and returns the next token.
        Token next(ParseContext& ctx) { return ctx.stream.next(); }

        /// @brief Consumes the next token, throwing if its value is not @p v.
        /// @param v The exact string the next token must match.
        Token expectValue(ParseContext& ctx, const char* v)
        {
            if (peek(ctx).value != v)
                error(ctx, "expected '" + std::string(v) + "' but got '" + peek(ctx).value + "'");
            return next(ctx);
        }

        /// @brief Consumes the next token, throwing if it is not an identifier.
        Token expectIdentifier(ParseContext& ctx)
        {
            if (peek(ctx).type != TokenType::Identifier)
                error(ctx, "expected identifier but got '" + peek(ctx).value + "'");
            return next(ctx);
        }

        /// @brief Throws a parse error at the current token position.
        /// @param msg Human-readable description of what went wrong.
        [[noreturn]] void error(ParseContext& ctx, const std::string& msg)
        {
            const Token& t = peek(ctx);
            throw std::runtime_error("VHDL parse error at line " + std::to_string(t.line) +
                ", column " + std::to_string(t.column) + ": " + msg);
        }

        /// @brief Discards tokens up to and including the next semicolon.
        void skipToSemicolon(ParseContext& ctx)
        {
            while (!ctx.stream.eof() && peek(ctx).value != ";") next(ctx);
            if (!ctx.stream.eof()) next(ctx);
        }

        /// @brief Parses and discards an "end [keyword] [name] ;" clause.
        /// @details Every VHDL end-clause is structurally "end <trailing stuff> ;",
        ///          so this simply skips tokens until the semicolon.
        void parseEndClause(ParseContext& ctx)
        {
            expectValue(ctx, "end");
            while (peek(ctx).value != ";") next(ctx);
            expectValue(ctx, ";");
        }

        /// @brief Returns true when @p v equals any of the @p n strings in @p opts.
        bool matchesAny(const std::string& v, const char* const* opts, size_t n)
        {
            for (size_t i = 0; i < n; ++i)
                if (v == opts[i]) return true;
            return false;
        }

        // ===============================================================
        // Entity / architecture / component
        // ===============================================================

        /// @brief Parses a full VHDL entity declaration.
        /// @details Registers the entity's port metadata in entityPortInfo so
        ///          that the matching architecture can resolve signal widths later.
        /// @return Ownership of the constructed EntityDeclaration node.
        std::unique_ptr<EntityDeclaration> parseEntity(ParseContext& ctx)
        {
            expectValue(ctx, "entity");
            std::string name = expectIdentifier(ctx).value;
            expectValue(ctx, "is");

            auto entity = std::make_unique<EntityDeclaration>();
            entity->entityName = name;

            std::unordered_map<std::string, SignalInfo> ports;
            parsePortList(ctx, entity->ports, ports);
            parseEndClause(ctx);

            ctx.entityPortInfo[name] = std::move(ports);
            return entity;
        }

        /// @brief Parses a component declaration inside an architecture.
        /// @details Port info from component declarations is intentionally discarded;
        ///          only the port names and directions are kept for instantiation checking.
        /// @return Ownership of the constructed ComponentDeclaration node.
        std::unique_ptr<ComponentDeclaration> parseComponentDeclaration(ParseContext& ctx)
        {
            expectValue(ctx, "component");
            std::string name = expectIdentifier(ctx).value;
            if (peek(ctx).value == "is") next(ctx);

            auto comp = std::make_unique<ComponentDeclaration>();
            comp->componentName = name;

            std::unordered_map<std::string, SignalInfo> discarded;
            parsePortList(ctx, comp->ports, discarded);
            parseEndClause(ctx);
            return comp;
        }

        /// @brief Parses an architecture body, including its declarative and statement parts.
        /// @details Ports of the referenced entity are imported into symbols so
        ///          that signal references inside the architecture body can be resolved.
        ///          The declarative part allows signal and component declarations.
        ///          The statement part allows concurrent signal assignments and
        ///          component instantiations.
        /// @return Ownership of the constructed ArchitectureDeclaration node.
        std::unique_ptr<ArchitectureDeclaration> parseArchitecture(ParseContext& ctx)
        {
            expectValue(ctx, "architecture");
            std::string archName = expectIdentifier(ctx).value;
            expectValue(ctx, "of");
            std::string entityName = expectIdentifier(ctx).value;
            expectValue(ctx, "is");

            auto arch = std::make_unique<ArchitectureDeclaration>();
            arch->architectureName = archName;
            arch->entityName = entityName;

            auto it = ctx.entityPortInfo.find(entityName);
            if (it == ctx.entityPortInfo.end())
                error(ctx, "architecture references unknown entity '" + entityName + "'");

            ctx.symbols = it->second; // ports are visible as signals inside the architecture

            // --- Declarative part (between "is" and "begin") ---
            while (true)
            {
                const std::string& v = peek(ctx).value;
                if (v == "signal")
                {
                    for (auto& decl : parseSignalDeclarations(ctx))
                        arch->signals.push_back(std::move(decl));
                }
                else if (v == "component")
                {
                    arch->components.push_back(parseComponentDeclaration(ctx));
                }
                else if (v == "begin")
                {
                    next(ctx);
                    break;
                }
                else
                {
                    error(ctx, "unexpected token in architecture declarative part: '" + v + "'");
                }
            }

            // --- Statement part (between "begin" and "end") ---
            while (true)
            {
                if (peek(ctx).value == "end")
                {
                    parseEndClause(ctx);
                    break;
                }

                // Labeled process: "label : process ..."
                if (peek(ctx).type == TokenType::Identifier && peek(ctx, 1).value == ":")
                {
                    const std::string label = peek(ctx).value;
                    if (peek(ctx, 2).value == "process")
                    {
                        next(ctx); // consume label
                        next(ctx); // consume ':'
                        arch->processes.push_back(parseProcess(ctx, label));
                        continue;
                    }
                    arch->instantiations.push_back(parseInstantiation(ctx));
                    continue;
                }

                // Unlabeled process
                if (peek(ctx).value == "process")
                {
                    arch->processes.push_back(parseProcess(ctx, ""));
                    continue;
                }

                arch->assignments.push_back(parseSignalAssignment(ctx));
            }

            return arch;
        }

        // ===============================================================
        // Port list / type spec
        // ===============================================================

        /// @brief Parses a "port ( ... );" clause and populates @p ports and @p infoOut.
        /// @param ports      Receives one PortDeclaration per declared port name.
        /// @param infoOut    Receives the corresponding SignalInfo for each port name,
        ///                   keyed by name (used for signal flattening later).
        void parsePortList(ParseContext& ctx, std::vector<std::unique_ptr<PortDeclaration>>& ports,
            std::unordered_map<std::string, SignalInfo>& infoOut)
        {
            expectValue(ctx, "port");
            expectValue(ctx, "(");
            while (true)
            {
                std::vector<std::string> names;
                names.push_back(expectIdentifier(ctx).value);
                while (peek(ctx).value == ",")
                {
                    next(ctx);
                    names.push_back(expectIdentifier(ctx).value);
                }
                expectValue(ctx, ":");

                bool isInput;
                if (peek(ctx).value == "in") { next(ctx); isInput = true; }
                else if (peek(ctx).value == "out") { next(ctx); isInput = false; }
                else error(ctx, "expected IN or OUT");

                TypeSpec spec = parseTypeSpec(ctx);

                for (auto& n : names)
                {
                    auto pd = std::make_unique<PortDeclaration>();
                    pd->portName = n;
                    pd->width = spec.width;
                    pd->isInput = isInput;
                    ports.push_back(std::move(pd));

                    infoOut[n] = SignalInfo{ spec.width, spec.isDownto, spec.low, spec.high, spec.isVector };
                }

                if (peek(ctx).value == ";")
                {
                    next(ctx);
                    if (peek(ctx).value == ")") break;
                    continue;
                }
                break;
            }
            expectValue(ctx, ")");
            expectValue(ctx, ";");
        }

        /// @brief Parses a VHDL type name: STD_LOGIC or STD_LOGIC_VECTOR(high DOWNTO/TO low).
        /// @details Validates that the direction keyword matches the bound ordering so that
        ///          malformed declarations are caught early.
        /// @return A TypeSpec describing width, direction, and bounds.
        TypeSpec parseTypeSpec(ParseContext& ctx)
        {
            Token t = next(ctx);
            if (t.value == "std_logic")
                return TypeSpec{ 1, false, true, 0, 0 };

            if (t.value != "std_logic_vector")
                error(ctx, "expected STD_LOGIC or STD_LOGIC_VECTOR, got '" + t.value + "'");

            expectValue(ctx, "(");
            int64_t first = parseConstIntExpr(ctx);

            bool isDownto;
            if (peek(ctx).value == "downto") { next(ctx); isDownto = true; }
            else if (peek(ctx).value == "to") { next(ctx); isDownto = false; }
            else { error(ctx, "expected DOWNTO or TO"); }

            int64_t second = parseConstIntExpr(ctx);
            expectValue(ctx, ")");

            if (isDownto && first < second)
                error(ctx, "vector declared with DOWNTO but first bound is less than second. Found: (" + std::to_string(first) + " downto " + std::to_string(second) + ")");
            if (!isDownto && first > second)
                error(ctx, "vector declared with TO but first bound is greater than second. Found: (" + std::to_string(first) + " to " + std::to_string(second) + ")");

            int64_t low = std::min(first, second);
            int64_t high = std::max(first, second);
            int64_t width = high - low + 1;
            if (width < 1 || width > 255)
                error(ctx, "vector width out of range");

            return TypeSpec{ static_cast<Pulse::bitWidth_t>(width), true, isDownto, low, high };
        }

        // ===============================================================
        // Signal declarations
        // ===============================================================

        /// @brief Parses one "signal name [, name]* : type [:= value] ;" declaration.
        /// @details All declared names share the same type and optional initial value.
        ///          Each name is registered in symbols for later reference resolution.
        /// @return A vector with one SignalDeclaration per name.
        std::vector<std::unique_ptr<SignalDeclaration>> parseSignalDeclarations(ParseContext& ctx)
        {
            expectValue(ctx, "signal");

            std::vector<std::string> names;
            names.push_back(expectIdentifier(ctx).value);
            while (peek(ctx).value == ",")
            {
                next(ctx);
                names.push_back(expectIdentifier(ctx).value);
            }
            expectValue(ctx, ":");

            TypeSpec spec = parseTypeSpec(ctx);

            uint64_t initialValue = 0;
            if (peek(ctx).value == ":=")
            {
                next(ctx);
                auto lit = parseLogicLiteralToken(ctx);
                initialValue = lit->value;
            }
            expectValue(ctx, ";");

            std::vector<std::unique_ptr<SignalDeclaration>> result;
            for (auto& n : names)
            {
                ctx.symbols[n] = SignalInfo{ spec.width, spec.isDownto, spec.low, spec.high, spec.isVector };

                auto decl = std::make_unique<SignalDeclaration>();
                decl->signalName = n;
                decl->width = spec.width;
                decl->initialValue = initialValue;
                result.push_back(std::move(decl));
            }
            return result;
        }

        // ===============================================================
        // Component instantiation
        // ===============================================================

        /// @brief Parses a labeled component instantiation statement.
        /// @details Handles both the "label : ComponentName port map (...)" form and
        ///          the "label : entity work.EntityName port map (...)" form.
        ///          Library prefixes are silently ignored.
        /// @return Ownership of the constructed ComponentInstantiation node.
        std::unique_ptr<ComponentInstantiation> parseInstantiation(ParseContext& ctx)
        {
            std::string instName = expectIdentifier(ctx).value;
            expectValue(ctx, ":");

            if (peek(ctx).value == "entity") next(ctx); // "entity work.foo" form - library prefix ignored

            std::string compName = expectIdentifier(ctx).value;
            if (peek(ctx).value == ".") // library.component form
            {
                next(ctx);
                compName = expectIdentifier(ctx).value;
            }

            expectValue(ctx, "port");
            expectValue(ctx, "map");
            expectValue(ctx, "(");

            auto inst = std::make_unique<ComponentInstantiation>();
            inst->instanceName = instName;
            inst->componentName = compName;

            while (true)
            {
                std::string portName = expectIdentifier(ctx).value;
                expectValue(ctx, "=>");
                auto actual = parseSignalReference(ctx);
                inst->portMaps[portName] = std::move(actual);

                if (peek(ctx).value == ",") { next(ctx); continue; }
                break;
            }
            expectValue(ctx, ")");
            expectValue(ctx, ";");
            return inst;
        }

        // ===============================================================
        // Signal assignment   target <= value [when cond else value ...] ;
        // ===============================================================

        /// @brief Parses a concurrent signal assignment statement, including
        ///        conditional (when/else) forms.
        /// @return Ownership of the constructed SignalAssignment node.
        std::unique_ptr<SignalAssignment> parseSignalAssignment(ParseContext& ctx)
        {
            auto target = parseSignalReference(ctx);
            expectValue(ctx, "<=");
            auto value = parseAssignmentValue(ctx);
            expectValue(ctx, ";");

            auto stmt = std::make_unique<SignalAssignment>();
            stmt->target = std::move(target);
            stmt->value = std::move(value);
            return stmt;
        }

        /// @brief Parses the right-hand side of a signal assignment.
        /// @details If the first value expression is followed by "when", a full
        ///          WhenElseExpr chain is built. Otherwise the value is returned as-is.
        ///
        ///          Grammar:
        ///          @code
        ///          value { when cond else value } else value
        ///          @endcode
        /// @return The parsed expression (possibly a WhenElseExpr).
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseAssignmentValue(ParseContext& ctx)
        {
            auto first = parseValueExpr(ctx);

            if (peek(ctx).value != "when")
                return first;
            next(ctx);

            auto whenExpr = std::make_unique<WhenElseExpr>();
            auto cond = parseCondition(ctx);
            whenExpr->branches.push_back(WhenElseExpr::Branch{ std::move(first), std::move(cond) });

            while (true)
            {
                expectValue(ctx, "else");
                auto val = parseValueExpr(ctx);
                if (peek(ctx).value == "when")
                {
                    next(ctx);
                    auto c = parseCondition(ctx);
                    whenExpr->branches.push_back(WhenElseExpr::Branch{ std::move(val), std::move(c) });
                }
                else
                {
                    whenExpr->defaultValue = std::move(val);
                    break;
                }
            }
            return whenExpr;
        }

        // ===============================================================
        // Process statements
        // ===============================================================

        /// @brief Parses a VHDL process statement.
        /// @details Grammar:
        /// @code
        ///   [label :] process [(sensitivity_list)]
        ///   begin
        ///     { sequential_statement }
        ///   end process [label] ;
        /// @endcode
        /// @param label The label attached to the process (empty string if none).
        /// @return Ownership of the constructed ProcessStatement node.
        std::unique_ptr<ProcessStatement> parseProcess(ParseContext& ctx, const std::string& label)
        {
            expectValue(ctx, "process");

            auto proc = std::make_unique<ProcessStatement>();
            proc->label = label;

            // Optional sensitivity list
            if (peek(ctx).value == "(")
            {
                next(ctx); // consume '('
                while (peek(ctx).value != ")")
                {
                    Token sig = expectIdentifier(ctx);
                    // Validate that the signal is known
                    if (ctx.symbols.find(sig.value) == ctx.symbols.end())
                        error(ctx, "undeclared signal '" + sig.value + "' in sensitivity list");
                    proc->sensitivityList.push_back(sig.value);
                    if (peek(ctx).value == ",") next(ctx);
                }
                expectValue(ctx, ")");
            }

            // Optional 'is' keyword (VHDL-2008 style)
            if (peek(ctx).value == "is") next(ctx);

            expectValue(ctx, "begin");

            proc->body = parseSequentialBody(ctx);

            // "end process [label] ;"
            expectValue(ctx, "end");
            if (peek(ctx).value == "process") next(ctx);
            if (peek(ctx).type == TokenType::Identifier) next(ctx); // optional label
            expectValue(ctx, ";");

            return proc;
        }

        /// @brief Parses zero or more sequential statements until a terminating keyword.
        /// @details Terminating keywords are: end, else, elsif.
        /// @return A vector of owned SequentialStatement nodes.
        std::vector<std::unique_ptr<SequentialStatement>> parseSequentialBody(ParseContext& ctx)
        {
            std::vector<std::unique_ptr<SequentialStatement>> stmts;
            while (true)
            {
                const std::string& v = peek(ctx).value;
                if (v == "end" || v == "else" || v == "elsif")
                    break;
                stmts.push_back(parseSequentialStatement(ctx));
            }
            return stmts;
        }

        /// @brief Parses a single sequential statement inside a process body.
        /// @details Dispatches to the appropriate sub-parser based on the next token:
        ///          - "if"   -> parseIfStatement()
        ///          - "wait" -> parseWaitForever()
        ///          - "wait for" -> parseWaitFor()
        ///          - otherwise an identifier starting a signal assignment
        /// @return Ownership of the constructed statement node.
        std::unique_ptr<SequentialStatement> parseSequentialStatement(ParseContext& ctx)
        {
            const std::string& v = peek(ctx).value;

            if (v == "if")
                return parseIfStatement(ctx);

            if (v == "wait") {
                if (peek(ctx, 1).value == "for") 
                    return parseWaitFor(ctx);
                else if (peek(ctx, 1).value == ";") 
                    return parseWaitForever(ctx);
                else
                    error(ctx, "unexpected token after 'wait': '" + peek(ctx, 1).value + "'");
            }

            // Signal assignment: identifier <= expr ;
            return parseSignalAssignment(ctx);
        }

        /// @brief Parses an if/elsif/else statement.
        /// @details Grammar:
        /// @code
        ///   if cond then body
        ///   { elsif cond then body }
        ///   [ else body ]
        ///   end if ;
        /// @endcode
        /// @return Ownership of the constructed IfStatement node.
        std::unique_ptr<IfStatement> parseIfStatement(ParseContext& ctx)
        {
            auto ifStmt = std::make_unique<IfStatement>();

            expectValue(ctx, "if");
            {
                IfStatement::Branch branch;
                branch.condition = parseCondition(ctx);
                expectValue(ctx, "then");
                branch.body = parseSequentialBody(ctx);
                ifStmt->branches.push_back(std::move(branch));
            }

            while (peek(ctx).value == "elsif")
            {
                next(ctx); // consume 'elsif'
                IfStatement::Branch branch;
                branch.condition = parseCondition(ctx);
                expectValue(ctx, "then");
                branch.body = parseSequentialBody(ctx);
                ifStmt->branches.push_back(std::move(branch));
            }

            if (peek(ctx).value == "else")
            {
                next(ctx); // consume 'else'
                ifStmt->elseBody = parseSequentialBody(ctx);
            }

            expectValue(ctx, "end");
            expectValue(ctx, "if");
            expectValue(ctx, ";");

            return ifStmt;
        }

        /// @brief Parses a "wait for <integer> [fs | ps | ns | us | ms] ;" statement.
        /// @return Ownership of the constructed WaitForStatement node.
        std::unique_ptr<WaitForStatement> parseWaitFor(ParseContext& ctx)
        {
            expectValue(ctx, "wait");
            expectValue(ctx, "for");

            int64_t duration = parseConstIntExpr(ctx);

            // Expect a time unit: fs, ps, ns, us, ms
            Token unit = next(ctx);
            int64_t multiplier = 0;
            if (unit.value == "fs") multiplier = 1LL;
            else if (unit.value == "ps") multiplier = 1000LL;
            else if (unit.value == "ns") multiplier = 1000000LL;
            else if (unit.value == "us") multiplier = 1000000000LL;
            else if (unit.value == "ms") multiplier = 1000000000000LL;
            else
                error(ctx, "expected time unit (fs, ps, ns, us, ms) after wait duration, got '" + unit.value + "'");

            expectValue(ctx, ";");

            auto stmt = std::make_unique<WaitForStatement>();
            stmt->durationFs = duration * multiplier;
            return stmt;
        }

        /// @brief Parses a "wait ;" statement that waits forever.
        /// @return Ownership of the constructed WaitForeverStatement node.
        std::unique_ptr<WaitForeverStatement> parseWaitForever(ParseContext& ctx)
        {
            expectValue(ctx, "wait");
            expectValue(ctx, ";");

            return std::make_unique<WaitForeverStatement>();
        }

        // ===============================================================
        // Signal references with flattened bit/range indexing
        // ===============================================================

        /// @brief Allocates a literal integer node with value @p v.
        std::unique_ptr<IntegerLiteralExpr> makeIntLit(int64_t v)
        {
            auto n = std::make_unique<IntegerLiteralExpr>();
            n->value = v;
            return n;
        }

        /// @brief Maps a VHDL-level declaration index to a flat internal position.
        /// @details Internal position 0 always corresponds to the bit written SECOND in
        ///          the declaration range (i.e. the rightmost element, the LSB), regardless
        ///          of whether the signal was declared with DOWNTO or TO.
        ///
        ///          For a DOWNTO declaration like (7 DOWNTO 0): internal = i - declaredLow
        ///          For a TO declaration    like (0 TO 7):      internal = declaredHigh - i
        ///
        /// @param info The SignalInfo for the signal being indexed.
        /// @param i    The VHDL-level index as written in the source.
        /// @return The equivalent flat (LSB=0) internal index.
        int64_t flattenIndex(const SignalInfo& info, int64_t i)
        {
            if (!info.isVector)
                return 0;
            return info.isDownto ? (i - info.declaredLow) : (info.declaredHigh - i);
        }

        /// @brief Parses a signal name with an optional bit-select or range-select.
        /// @details If an index or range follows in parentheses, the bounds are flattened
        ///          from VHDL declaration space into internal LSB=0 space via flattenIndex().
        ///
        ///          Grammar:
        ///          @code
        ///          identifier [ "(" index ")" | "(" high (DOWNTO|TO) low ")" ]
        ///          @endcode
        /// @return Ownership of the constructed SignalReference node.
        std::unique_ptr<SignalReference> parseSignalReference(ParseContext& ctx)
        {
            Token nameTok = expectIdentifier(ctx);
            auto it = ctx.symbols.find(nameTok.value);
            if (it == ctx.symbols.end())
                error(ctx, "undeclared signal '" + nameTok.value + "'");
            const SignalInfo& info = it->second;

            auto ref = std::make_unique<SignalReference>();
            ref->signalName = nameTok.value;

            if (peek(ctx).value == "(")
            {
                next(ctx);
                int64_t firstIdx = parseConstIntExpr(ctx);

                bool isDownto = peek(ctx).value == "downto";
                bool isTo = peek(ctx).value == "to";

                if (isDownto || isTo)
                {
                    next(ctx);
                    int64_t secondIdx = parseConstIntExpr(ctx);

                    if (isDownto && firstIdx < secondIdx)
                        error(ctx, "vector declared with DOWNTO but first bound is less than second. Found: (" + std::to_string(firstIdx) + " downto " + std::to_string(secondIdx) + ")");
                    if (isTo && firstIdx > secondIdx)
                        error(ctx, "vector declared with TO but first bound is greater than second. Found: (" + std::to_string(firstIdx) + " to " + std::to_string(secondIdx) + ")");

                    expectValue(ctx, ")");
                    ref->high = makeIntLit(flattenIndex(info, firstIdx));
                    ref->low = makeIntLit(flattenIndex(info, secondIdx));
                }
                else
                {
                    expectValue(ctx, ")");
                    ref->low = makeIntLit(flattenIndex(info, firstIdx));
                }
            }

            return ref;
        }

        // ===============================================================
        // Constant integer expressions (folded immediately)
        // ===============================================================

        /// @brief Parses and immediately evaluates a constant integer expression.
        /// @details Handles additive operators at the top level, delegating
        ///          multiplication and primaries to parseConstMul() / parseConstFactor().
        /// @return The folded integer value.
        int64_t parseConstIntExpr(ParseContext& ctx)
        {
            int64_t v = parseConstMul(ctx);
            while (peek(ctx).value == "+" || peek(ctx).value == "-")
            {
                bool plus = (next(ctx).value == "+");
                int64_t r = parseConstMul(ctx);
                v = plus ? v + r : v - r;
            }
            return v;
        }

        /// @brief Parses and evaluates the multiplicative level of a constant expression.
        /// @return The folded integer value for this sub-expression.
        int64_t parseConstMul(ParseContext& ctx)
        {
            int64_t v = parseConstFactor(ctx);
            while (peek(ctx).value == "*")
            {
                next(ctx);
                v *= parseConstFactor(ctx);
            }
            return v;
        }

        /// @brief Parses and evaluates a constant primary: a numeric literal, a
        ///        parenthesised expression, or a signal attribute (e.g. A'length).
        /// @return The folded integer value for this primary.
        int64_t parseConstFactor(ParseContext& ctx)
        {
            if (peek(ctx).value == "-") { next(ctx); return -parseConstFactor(ctx); }
            if (peek(ctx).value == "+") { next(ctx); return parseConstFactor(ctx); }
            if (peek(ctx).value == "(")
            {
                next(ctx);
                int64_t v = parseConstIntExpr(ctx);
                expectValue(ctx, ")");
                return v;
            }

            Token t = next(ctx);
            if (t.type == TokenType::NumericLiteral)
                return parseIntegerLiteralText(t.value);

            if (t.type == TokenType::Identifier)
            {
                if (peek(ctx).value == "'") next(ctx); // tokenizer may emit the tick separately
                Token attrTok = next(ctx);
                if (attrTok.type != TokenType::Attribute)
                    error(ctx, "expected attribute after '" + t.value + "'");
                return evalConstAttribute(ctx, t.value, attrTok.value);
            }

            error(ctx, "expected integer constant, got '" + t.value + "'");
        }

        /// @brief Evaluates a signal attribute expression to a concrete integer.
        /// @details Recognised attributes: 'length, 'high, 'low, 'left, 'right.
        /// @param signalName Name of the signal whose attribute is being queried.
        /// @param attr       Attribute name, with or without the leading tick.
        /// @return The integer value of the attribute for the given signal.
        int64_t evalConstAttribute(ParseContext& ctx, const std::string& signalName, std::string attr)
        {
            if (!attr.empty() && attr.front() == '\'') attr.erase(0, 1);

            auto it = ctx.symbols.find(signalName);
            if (it == ctx.symbols.end())
                error(ctx, "unknown signal in attribute expression: '" + signalName + "'");
            const SignalInfo& info = it->second;

            if (attr == "length") return info.width;
            if (!info.isVector)
                error(ctx, "'" + attr + " is not valid on scalar signal '" + signalName + "'");

            if (attr == "high") return info.declaredHigh;
            if (attr == "low") return info.declaredLow;
            if (attr == "left") return info.isDownto ? info.declaredHigh : info.declaredLow;
            if (attr == "right") return info.isDownto ? info.declaredLow : info.declaredHigh;

            error(ctx, "unsupported attribute '" + attr + "'");
        }

        // ===============================================================
        // Integer sub-expressions kept as AST (shift amounts, etc.)
        // ===============================================================

        /// @brief Parses an integer-valued expression and returns it as an AST subtree.
        /// @details Unlike parseConstIntExpr, the result is not folded to a constant;
        ///          it may reference signals at runtime (e.g. for shift amounts).
        /// @return Ownership of the AST node for this expression.
        std::unique_ptr<ASTNode> parseIntegerExpr(ParseContext& ctx)
        {
            auto v = parseIntMul(ctx);
            while (peek(ctx).value == "+" || peek(ctx).value == "-")
            {
                std::string op = next(ctx).value;
                auto r = parseIntMul(ctx);
                auto node = std::make_unique<BinaryOpExpr<ReturnType::INTEGER>>();
                node->op = op;
                node->left = std::move(v);
                node->right = std::move(r);
                v = std::move(node);
            }
            return v;
        }

        /// @brief Parses the multiplicative level of an AST integer expression.
        /// @return Ownership of the AST node for this sub-expression.
        std::unique_ptr<ASTNode> parseIntMul(ParseContext& ctx)
        {
            auto v = parseIntFactor(ctx);
            while (peek(ctx).value == "*")
            {
                next(ctx);
                auto r = parseIntFactor(ctx);
                auto node = std::make_unique<BinaryOpExpr<ReturnType::INTEGER>>();
                node->op = "*";
                node->left = std::move(v);
                node->right = std::move(r);
                v = std::move(node);
            }
            return v;
        }

        /// @brief Parses a primary integer expression into an AST node.
        /// @details Handles four forms:
        ///            - Unary minus:                   "-" factor
        ///            - Parenthesised sub-expression:  "(" expr ")"
        ///            - Numeric literal:               e.g. 42
        ///            - Signal attribute:              ident "'" attr  (e.g. A'length)
        ///            - Signal with subscript:         ident "(" ... ")" or plain ident
        ///
        /// @note The last form (plain signal / subscripted signal) is required to support
        ///       shift amounts written as a signal slice, e.g. "A SLL B(5 DOWNTO 0)".
        ///       In that case the subscript is already consumed by parseSignalReference(),
        ///       and the resulting SignalReference is used directly as the shift amount.
        ///       Without this branch, parsing "B(5 DOWNTO 0)" as the shift amount would
        ///       fail because the parser would try to consume a tick-attribute after "B"
        ///       and instead find "(", producing a spurious error.
        /// @return Ownership of the AST node for this primary.
        std::unique_ptr<ASTNode> parseIntFactor(ParseContext& ctx)
        {
            if (peek(ctx).value == "-")
            {
                next(ctx);
                auto operand = parseIntFactor(ctx);
                auto node = std::make_unique<UnaryOpExpr<ReturnType::INTEGER>>();
                node->op = "-";
                node->operand = std::move(operand);
                return node;
            }

            if (peek(ctx).value == "(")
            {
                next(ctx);
                auto e = parseIntegerExpr(ctx);
                expectValue(ctx, ")");
                return e;
            }

            Token t = next(ctx);

            if (t.type == TokenType::NumericLiteral)
            {
                auto n = std::make_unique<IntegerLiteralExpr>();
                n->value = parseIntegerLiteralText(t.value);
                return n;
            }

            if (t.type == TokenType::Identifier)
            {
                // Two sub-cases for an identifier primary:
                //
                //   a) signal'attribute  — the token after the identifier is a tick,
                //      or directly a TokenType::Attribute.  Produce an AttributeExpr.
                //
                //   b) signal reference  — the token after the identifier is "(" or
                //      anything else (end of expression).  Parse the full signal
                //      reference (including any subscript) and return it as-is.
                //      This correctly handles shift amounts like "B(5 DOWNTO 0)".

                const bool nextIsTick = (peek(ctx).value == "'");
                const bool nextIsAttr = (peek(ctx).type == TokenType::Attribute);

                if (nextIsTick || nextIsAttr)
                {
                    // Attribute form: signal'attr
                    if (nextIsTick) next(ctx); // consume the tick if emitted separately
                    Token attrTok = next(ctx);
                    if (attrTok.type != TokenType::Attribute)
                        error(ctx, "expected attribute after '" + t.value + "'");

                    auto n = std::make_unique<AttributeExpr>();
                    n->kind = attributeKindFromString(attrTok.value);
                    n->target = std::make_unique<SignalReference>();
                    n->target->signalName = t.value;
                    return n;
                }

                // Plain signal reference (with optional subscript).
                // We already consumed the identifier token, so we push it back by
                // looking up the signal directly rather than calling parseSignalReference().
                auto it = ctx.symbols.find(t.value);
                if (it == ctx.symbols.end())
                    error(ctx, "undeclared signal '" + t.value + "'");
                const SignalInfo& info = it->second;

                auto ref = std::make_unique<SignalReference>();
                ref->signalName = t.value;

                if (peek(ctx).value == "(")
                {
                    next(ctx);
                    int64_t firstIdx = parseConstIntExpr(ctx);

                    bool isDownto = peek(ctx).value == "downto";
                    bool isTo = peek(ctx).value == "to";

                    if (isDownto || isTo)
                    {
                        next(ctx);
                        int64_t secondIdx = parseConstIntExpr(ctx);

                        if (isDownto && firstIdx < secondIdx)
                            error(ctx, "slice DOWNTO but first bound is less than second. Found: (" + std::to_string(firstIdx) + " downto " + std::to_string(secondIdx) + ")");
                        if (isTo && firstIdx > secondIdx)
                            error(ctx, "slice TO but first bound is greater than second. Found: (" + std::to_string(firstIdx) + " to " + std::to_string(secondIdx) + ")");

                        expectValue(ctx, ")");
                        ref->high = makeIntLit(flattenIndex(info, firstIdx));
                        ref->low = makeIntLit(flattenIndex(info, secondIdx));
                    }
                    else
                    {
                        expectValue(ctx, ")");
                        ref->low = makeIntLit(flattenIndex(info, firstIdx));
                    }
                }

                return ref;
            }

            error(ctx, "expected integer expression, got '" + t.value + "'");
        }

        /// @brief Converts an attribute name string to the corresponding AttributeKind enum value.
        /// @details The leading tick, if present, is stripped before comparison.
        ///          Throws if the attribute name is not one of the standard VHDL signal attributes.
        /// @param s The raw attribute string (e.g. "'length", "high").
        /// @return The matching AttributeKind enumerator.
        AttributeKind attributeKindFromString(std::string s)
        {
            if (!s.empty() && s.front() == '\'') s.erase(0, 1);
            if (s == "left") return AttributeKind::Left;
            if (s == "right") return AttributeKind::Right;
            if (s == "low") return AttributeKind::Low;
            if (s == "high") return AttributeKind::High;
            if (s == "length") return AttributeKind::Length;
            throw std::runtime_error("unsupported attribute '" + s + "'");
        }

        // ===============================================================
        // LOGIC-valued expressions (signal assignment RHS)
        // ===============================================================

        /// @brief Entry point for a LOGIC-valued expression. Delegates to the lowest
        ///        precedence level (logical operators).
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseValueExpr(ParseContext& ctx)
        {
            return parseLogicalLevel(ctx);
        }

        /// @brief Parses the logical operator level (and/or/nand/nor/xor/xnor).
        /// @details Left-associative. All operands and results are LOGIC-typed.
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseLogicalLevel(ParseContext& ctx)
        {
            static const char* ops[] = { "and", "or", "nand", "nor", "xor", "xnor" };
            auto left = parseShiftLevel(ctx);
            while (matchesAny(peek(ctx).value, ops, 6))
            {
                std::string op = next(ctx).value;
                auto right = parseShiftLevel(ctx);
                left = wrapBinary(op, std::move(left), std::move(right));
            }
            return left;
        }

        /// @brief Parses the shift/rotate operator level (sll/srl/sla/sra/rol/ror).
        /// @details The right operand is an integer expression (not a LOGIC expression)
        ///          because shift amounts are numeric, not bit-vectors.
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseShiftLevel(ParseContext& ctx)
        {
            static const char* ops[] = { "sll", "srl", "sla", "sra", "rol", "ror" };
            auto left = parseAddingLevel(ctx);
            while (matchesAny(peek(ctx).value, ops, 6))
            {
                std::string op = next(ctx).value;
                auto amount = parseIntegerExpr(ctx);
                auto node = std::make_unique<BinaryOpExpr<ReturnType::LOGIC>>();
                node->op = op;
                node->left = std::move(left);
                node->right = std::move(amount);
                left = std::move(node);
            }
            return left;
        }

        /// @brief Parses the adding operator level (+ - &).
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseAddingLevel(ParseContext& ctx)
        {
            auto left = parseUnaryLevel(ctx);
            while (peek(ctx).value == "+" || peek(ctx).value == "-" || peek(ctx).value == "&")
            {
                std::string op = next(ctx).value;
                auto right = parseUnaryLevel(ctx);
                left = wrapBinary(op, std::move(left), std::move(right));
            }
            return left;
        }

        /// @brief Parses unary operators (not / + / -).
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseUnaryLevel(ParseContext& ctx)
        {
            if (peek(ctx).value == "not")
            {
                next(ctx);
                auto operand = parseUnaryLevel(ctx);
                auto node = std::make_unique<UnaryOpExpr<ReturnType::LOGIC>>();
                node->op = "not";
                node->operand = std::move(operand);
                return node;
            }
            if (peek(ctx).value == "+" || peek(ctx).value == "-")
            {
                std::string op = next(ctx).value;
                auto operand = parseUnaryLevel(ctx);
                auto node = std::make_unique<UnaryOpExpr<ReturnType::LOGIC>>();
                node->op = op;
                node->operand = std::move(operand);
                return node;
            }
            return parseMultLevel(ctx);
        }

        /// @brief Parses the multiplication level (*).
        std::unique_ptr<Expression<ReturnType::LOGIC>> parseMultLevel(ParseContext& ctx)
        {
            auto left = parsePrimary(ctx);
            while (peek(ctx).value == "*")
            {
                next(ctx);
                auto right = parsePrimary(ctx);
                left = wrapBinary("*", std::move(left), std::move(right));
            }
            return left;
        }

        /// @brief Parses a primary LOGIC expression: parenthesised sub-expression,
        ///        signed()/unsigned() cast, bit-string/character literal, or signal reference.
        /// @details To add a new built-in function, follow the signed/unsigned pattern:
        ///          check for the keyword in peek(), consume it, parse arguments,
        ///          and emit a FunctionCallExpr node.
        std::unique_ptr<Expression<ReturnType::LOGIC>> parsePrimary(ParseContext& ctx)
        {
            const Token& t = peek(ctx);

            if (t.value == "(")
            {
                next(ctx);
                auto e = parseValueExpr(ctx);
                expectValue(ctx, ")");
                return e;
            }

            if (t.value == "signed" || t.value == "unsigned")
            {
                std::string fname = next(ctx).value;
                expectValue(ctx, "(");
                auto arg = parseValueExpr(ctx);
                expectValue(ctx, ")");

                auto node = std::make_unique<FunctionCallExpr<ReturnType::LOGIC>>();
                node->functionName = fname;
                node->arguments.push_back(std::move(arg));
                return node;
            }

            if (t.type == TokenType::BitStringLiteral ||
                t.type == TokenType::CharacterLiteral)
                return parseLogicLiteralToken(ctx);

            if (t.type == TokenType::Identifier)
                return parseSignalReference(ctx);

            error(ctx, "unexpected token in expression: '" + t.value + "'");
        }

        /// @brief Constructs a BinaryOpExpr from an operator string and two sub-expressions.
        /// @param op    The operator name (e.g. "and", "+", "sll").
        /// @param left  Left operand.
        /// @param right Right operand.
        /// @return The constructed binary expression node.
        std::unique_ptr<Expression<ReturnType::LOGIC>> wrapBinary(
            const std::string& op,
            std::unique_ptr<Expression<ReturnType::LOGIC>> left,
            std::unique_ptr<Expression<ReturnType::LOGIC>> right)
        {
            auto node = std::make_unique<BinaryOpExpr<ReturnType::LOGIC>>();
            node->op = op;
            node->left = std::move(left);
            node->right = std::move(right);
            return node;
        }

        // ===============================================================
        // BOOLEAN-valued expressions (when/else conditions)
        // ===============================================================

        /// @brief Parses a boolean condition expression for a when/else clause.
        /// @details Handles logical combinations of relational comparisons.
        ///          To add a new logical operator, extend the ops table below.
        std::unique_ptr<Expression<ReturnType::BOOLEAN>> parseCondition(ParseContext& ctx)
        {
            static const char* ops[] = { "and", "or", "nand", "nor", "xor", "xnor" };
            auto left = parseBoolTerm(ctx);
            while (matchesAny(peek(ctx).value, ops, 6))
            {
                std::string op = next(ctx).value;
                auto right = parseBoolTerm(ctx);
                auto node = std::make_unique<BinaryOpExpr<ReturnType::BOOLEAN>>();
                node->op = op;
                node->left = std::move(left);
                node->right = std::move(right);
                left = std::move(node);
            }
            return left;
        }

        /// @brief Parses a boolean primary: a "not" term, a parenthesised condition,
        ///        or a LOGIC expression followed by a relational operator and another
        ///        LOGIC expression.
        std::unique_ptr<Expression<ReturnType::BOOLEAN>> parseBoolTerm(ParseContext& ctx)
        {
            if (peek(ctx).value == "not")
            {
                next(ctx);
                auto operand = parseBoolTerm(ctx);
                auto node = std::make_unique<UnaryOpExpr<ReturnType::BOOLEAN>>();
                node->op = "not";
                node->operand = std::move(operand);
                return node;
            }
            if (peek(ctx).value == "(")
            {
                next(ctx);
                auto e = parseCondition(ctx);
                expectValue(ctx, ")");
                return e;
            }

            auto left = parseValueExpr(ctx);

            static const char* relops[] = { "=", "/=", "<", "<=", ">", ">=" };
            if (matchesAny(peek(ctx).value, relops, 6))
            {
                std::string op = next(ctx).value;
                auto right = parseValueExpr(ctx);
                auto node = std::make_unique<BinaryOpExpr<ReturnType::BOOLEAN>>();
                node->op = op;
                node->left = std::move(left);
                node->right = std::move(right);
                return node;
            }

            error(ctx, "expected relational operator in condition");
        }

        // ===============================================================
        // Literals
        // ===============================================================

        /// @brief Parses a single logic literal token (bit-string or character).
        /// @details Dispatches to bitStringToLiteral() or charToLiteral() based on
        ///          the token type.
        /// @return Ownership of the constructed LogicLiteralExpr node.
        std::unique_ptr<LogicLiteralExpr> parseLogicLiteralToken(ParseContext& ctx)
        {
            Token t = next(ctx);
            if (t.type == TokenType::BitStringLiteral) return bitStringToLiteral(ctx, t.value);
            if (t.type == TokenType::CharacterLiteral) return charToLiteral(ctx, t.value);
            error(ctx, "expected a STD_LOGIC literal, got '" + t.value + "'");
        }

        /// @brief Converts a VHDL bit-string literal (e.g. b"1010", x"F", o"7") to a
        ///        LogicLiteralExpr, preserving any unknown/don't-care bits in unknownMask.
        /// @details Supports binary (b), octal (o), and hexadecimal (x) radix prefixes.
        ///          Underscore separators inside the digit string are ignored.
        ///          Unknown digit values (X/Z/U/W/-/L/H) set the corresponding mask bits.
        /// @param raw The raw token text as returned by the tokenizer.
        /// @return Ownership of the constructed literal node.
        std::unique_ptr<LogicLiteralExpr> bitStringToLiteral(ParseContext& ctx, const std::string& raw)
        {
            size_t i = 0;
            char radix = 'b';

            if (i < raw.size() && std::isalpha(static_cast<unsigned char>(raw[i])))
            {
                radix = static_cast<char>(static_cast<unsigned char>(raw[i]));
                ++i;
            }

            std::string digits;
            size_t quote = raw.find('"', i);
            if (quote != std::string::npos)
            {
                size_t close = raw.rfind('"');
                digits = raw.substr(quote + 1, close - quote - 1);
            }
            else
            {
                digits = raw.substr(i);
            }

            int bitsPerDigit = (radix == 'x') ? 4 : (radix == 'o') ? 3 : 1;

            uint64_t value = 0, mask = 0;
            int width = 0;
            for (char c : digits)
            {
                if (c == '_') continue;
                char lc = static_cast<char>(static_cast<unsigned char>(c));

                int digitVal = 0;
                bool unknown = false;
                if (radix == 'b')
                {
                    if (lc == '0') digitVal = 0;
                    else if (lc == '1') digitVal = 1;
                    else unknown = true;
                }
                else if (lc >= '0' && lc <= '9')
                {
                    digitVal = lc - '0';
                }
                else if (radix == 'x' && lc >= 'a' && lc <= 'f')
                {
                    digitVal = lc - 'a' + 10;
                }
                else
                {
                    unknown = true;
                }

                for (int b = bitsPerDigit - 1; b >= 0; --b)
                {
                    value <<= 1;
                    mask <<= 1;
                    if (unknown) mask |= 1ULL;
                    else value |= static_cast<uint64_t>((digitVal >> b) & 1);
                    ++width;
                }
            }

            if (width > 64) error(ctx, "bit string literal wider than 64 bits is not supported");

            auto lit = std::make_unique<LogicLiteralExpr>();
            lit->value = value;
            lit->unknownMask = mask;
            lit->width = static_cast<Pulse::bitWidth_t>(width);
            return lit;
        }

        /// @brief Converts a VHDL character literal ('0', '1', 'X', etc.) to a
        ///        one-bit LogicLiteralExpr.
        /// @details '0' and '1' produce known values. Any other character (X/Z/U/W/-/L/H)
        ///          is treated as an unknown bit (unknownMask = 1).
        /// @param raw The raw token text, e.g. "'0'" or "1".
        /// @return Ownership of the constructed literal node.
        std::unique_ptr<LogicLiteralExpr> charToLiteral(ParseContext& ctx, const std::string& raw)
        {
            char c;
            if (raw.size() >= 3 && raw.front() == '\'' && raw.back() == '\'') c = raw[1];
            else if (!raw.empty()) c = raw[0];
            else error(ctx, "empty character literal");

            char lc = static_cast<char>(static_cast<unsigned char>(c));

            auto lit = std::make_unique<LogicLiteralExpr>();
            lit->width = 1;
            if (lc == '0') { lit->value = 0; lit->unknownMask = 0; }
            else if (lc == '1') { lit->value = 1; lit->unknownMask = 0; }
            else { lit->value = 0; lit->unknownMask = 1; } // X/Z/U/W/-/L/H -> unknown
            return lit;
        }

        // ===============================================================
        // Main Parse Routine
        // ===============================================================
        
        /// @brief Entry point. Parses an entire VHDL source file and returns its root node.
        /// @details Skips library/use clauses then dispatches to parseEntity() and
        ///          parseArchitecture() for every top-level construct found.
        /// @return The populated root node for the whole file.
        ASTRoot parse(ParseContext& ctx)
        {
            ASTRoot r;
            while (!ctx.stream.eof())
            {
                const std::string& v = peek(ctx).value;
                if (v == "library" || v == "use")
                {
                    skipToSemicolon(ctx);
                    continue;
                }
                if (v == "entity")
                {
                    r.children.push_back(parseEntity(ctx));
                    continue;
                }
                if (v == "architecture")
                {
                    r.children.push_back(parseArchitecture(ctx));
                    continue;
                }
                error(ctx, "unexpected top-level token '" + v + "'");
            }
            return r;
        }

    } // namespace

    [[nodiscard]]
    ASTRoot VHDLtoAST(Tokenizer& tokenizer)
    {
        ParseContext ctx(tokenizer);
        return parse(ctx);
    }
    
} // namespace Pulse::Parser::VHDL