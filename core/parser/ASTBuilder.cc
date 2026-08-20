#include "ASTBuilder.h"

#include <cctype>
#include <cstring>
#include <deque>
#include <algorithm>

namespace Pulse::Parser::VHDL
{
    namespace
    {
        // -------------------------------------------------------------------
        // Small helpers
        // -------------------------------------------------------------------

        std::string toLower(std::string s)
        {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        int64_t parseIntegerLiteralText(const std::string& raw)
        {
            std::string digits;
            digits.reserve(raw.size());
            for (char c : raw)
                if (c != '_') digits.push_back(c);
            return std::stoll(digits);
        }

        class TokenStream
        {
        public:
            explicit TokenStream(Tokenizer& src) : m_src(src) { }

            const Token& peek(size_t n = 0)
            {
                fill(n);
                if (n < m_buffer.size()) return m_buffer[n];
                static const Token eofToken{ TokenType::Unknown, "", 0, 0 };
                return eofToken;
            }

            Token next()
            {
                fill(0);
                if (m_buffer.empty())
                    throw std::runtime_error("VHDL parse error: unexpected end of input");
                Token t = std::move(m_buffer.front());
                m_buffer.pop_front();
                return t;
            }

            bool eof()
            {
                fill(0);
                return m_buffer.empty() && m_ended;
            }

        private:
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

        // -------------------------------------------------------------------
        // Internal bookkeeping for a declared signal/port. Never lives in the
        // tree - only used to normalize/flatten bit and range accesses.
        // -------------------------------------------------------------------
        struct SignalInfo
        {
            Pulse::bitWidth_t width = 1;
            bool isDownto = true;     // declaration direction (irrelevant for scalars)
            int64_t declaredLow = 0;  // min(bound0, bound1)
            int64_t declaredHigh = 0; // max(bound0, bound1)
            bool isVector = false;
        };

        struct TypeSpec
        {
            Pulse::bitWidth_t width;
            bool isVector;
            bool isDownto;
            int64_t low;
            int64_t high;
        };

        // -------------------------------------------------------------------
        // The actual recursive-descent parser.
        // -------------------------------------------------------------------
        class Parser
        {
        public:
            explicit Parser(Tokenizer& tokenizer) : m_stream(tokenizer) { }

            RootNode parse()
            {
                RootNode r;
                while (!m_stream.eof())
                {
                    const std::string& v = m_stream.peek().value;
                    if (v == "library" || v == "use")
                    {
                        skipToSemicolon();
                        continue;
                    }
                    if (v == "entity")
                    {
                        r.children.push_back(parseEntity());
                        continue;
                    }
                    if (v == "architecture")
                    {
                        r.children.push_back(parseArchitecture());
                        continue;
                    }
                    error("unexpected top-level token '" + v + "'");
                }
                return r;
            }

        private:
            // ---------------------------------------------------------------
            // Token helpers
            // ---------------------------------------------------------------

            const Token& peek(size_t n = 0) { return m_stream.peek(n); }
            Token next() { return m_stream.next(); }

            Token expectValue(const char* v)
            {
                if (peek().value != v)
                    error("expected '" + std::string(v) + "' but got '" + peek().value + "'");
                return next();
            }

            Token expectIdentifier()
            {
                if (peek().type != TokenType::Identifier)
                    error("expected identifier but got '" + peek().value + "'");
                return next();
            }

            [[noreturn]] void error(const std::string& msg)
            {
                const Token& t = peek();
                throw std::runtime_error("VHDL parse error at line " + std::to_string(t.line) +
                    ", column " + std::to_string(t.column) + ": " + msg);
            }

            void skipToSemicolon()
            {
                while (!m_stream.eof() && peek().value != ";") next();
                if (!m_stream.eof()) next();
            }

            // Swallows "end [entity|architecture|component|<name>] ;" in one go -
            // every valid VHDL "end" form is just "end" followed by arbitrary
            // trailing tokens up to the semicolon.
            void parseEndClause()
            {
                expectValue("end");
                while (peek().value != ";") next();
                expectValue(";");
            }

            // ---------------------------------------------------------------
            // Entities / architectures / components
            // ---------------------------------------------------------------

            std::unique_ptr<EntityDeclaration> parseEntity()
            {
                expectValue("entity");
                std::string name = expectIdentifier().value;
                expectValue("is");

                auto entity = std::make_unique<EntityDeclaration>();
                entity->entityName = name;

                std::unordered_map<std::string, SignalInfo> ports;
                parsePortList(entity->ports, ports);
                parseEndClause();

                m_entityPortInfo[name] = std::move(ports);
                return entity;
            }

            std::unique_ptr<ComponentDeclaration> parseComponentDeclaration()
            {
                expectValue("component");
                std::string name = expectIdentifier().value;
                if (peek().value == "is") next();

                auto comp = std::make_unique<ComponentDeclaration>();
                comp->componentName = name;

                std::unordered_map<std::string, SignalInfo> discarded;
                parsePortList(comp->ports, discarded);
                parseEndClause();
                return comp;
            }

            std::unique_ptr<ArchitectureDeclaration> parseArchitecture()
            {
                expectValue("architecture");
                std::string archName = expectIdentifier().value;
                expectValue("of");
                std::string entityName = expectIdentifier().value;
                expectValue("is");

                auto arch = std::make_unique<ArchitectureDeclaration>();
                arch->architectureName = archName;
                arch->entityName = entityName;

                auto it = m_entityPortInfo.find(entityName);
                if (it == m_entityPortInfo.end())
                    error("architecture references unknown entity '" + entityName + "'");

                m_symbols = it->second; // ports are visible as signals inside the architecture

                // Declarative part
                while (true)
                {
                    const std::string& v = peek().value;
                    if (v == "signal")
                    {
                        for (auto& decl : parseSignalDeclarations())
                            arch->signals.push_back(std::move(decl));
                    }
                    else if (v == "component")
                    {
                        arch->components.push_back(parseComponentDeclaration());
                    }
                    else if (v == "begin")
                    {
                        next();
                        break;
                    }
                    else
                    {
                        error("unexpected token in architecture declarative part: '" + v + "'");
                    }
                }

                // Statement part
                while (true)
                {
                    if (peek().value == "end")
                    {
                        parseEndClause();
                        break;
                    }

                    if (peek().type == TokenType::Identifier && peek(1).value == ":")
                        arch->instantiations.push_back(parseInstantiation());
                    else
                        arch->assignments.push_back(parseSignalAssignment());
                }

                return arch;
            }

            void parsePortList(std::vector<std::unique_ptr<PortDeclaration>>& ports,
                std::unordered_map<std::string, SignalInfo>& infoOut)
            {
                expectValue("port");
                expectValue("(");
                while (true)
                {
                    std::vector<std::string> names;
                    names.push_back(expectIdentifier().value);
                    while (peek().value == ",")
                    {
                        next();
                        names.push_back(expectIdentifier().value);
                    }
                    expectValue(":");

                    bool isInput;
                    if (peek().value == "in") { next(); isInput = true; }
                    else if (peek().value == "out") { next(); isInput = false; }
                    else error("expected IN or OUT");

                    TypeSpec spec = parseTypeSpec();

                    for (auto& n : names)
                    {
                        auto pd = std::make_unique<PortDeclaration>();
                        pd->portName = n;
                        pd->width = spec.width;
                        pd->isInput = isInput;
                        ports.push_back(std::move(pd));

                        infoOut[n] = SignalInfo{ spec.width, spec.isDownto, spec.low, spec.high, spec.isVector };
                    }

                    if (peek().value == ";")
                    {
                        next();
                        if (peek().value == ")") break;
                        continue;
                    }
                    break;
                }
                expectValue(")");
                expectValue(";");
            }

            TypeSpec parseTypeSpec()
            {
                Token t = next();
                if (t.value == "std_logic")
                    return TypeSpec{ 1, false, true, 0, 0 };

                if (t.value != "std_logic_vector")
                    error("expected STD_LOGIC or STD_LOGIC_VECTOR, got '" + t.value + "'");

                expectValue("(");
                int64_t first = parseConstIntExpr();

                bool isDownto;
                if (peek().value == "downto") { next(); isDownto = true; }
                else if (peek().value == "to") { next(); isDownto = false; }
                else { error("expected DOWNTO or TO"); }

                int64_t second = parseConstIntExpr();
                expectValue(")");

                if (isDownto && first < second)
                    error("vector declared with DOWNTO but first bound is less than second. Found: (" + std::to_string(first) + " downto " + std::to_string(second) + ")");
                if (!isDownto && first > second)
                    error("vector declared with TO but first bound is greater than second. Found: (" + std::to_string(first) + " to " + std::to_string(second) + ")");

                int64_t low = std::min(first, second);
                int64_t high = std::max(first, second);
                int64_t width = high - low + 1;
                if (width < 1 || width > 255)
                    error("vector width out of range");

                return TypeSpec{ static_cast<Pulse::bitWidth_t>(width), true, isDownto, low, high };
            }

            // ---------------------------------------------------------------
            // Signal declarations
            // ---------------------------------------------------------------

            std::vector<std::unique_ptr<SignalDeclaration>> parseSignalDeclarations()
            {
                expectValue("signal");

                std::vector<std::string> names;
                names.push_back(expectIdentifier().value);
                while (peek().value == ",")
                {
                    next();
                    names.push_back(expectIdentifier().value);
                }
                expectValue(":");

                TypeSpec spec = parseTypeSpec();

                uint64_t initialValue = 0;
                if (peek().value == ":=")
                {
                    next();
                    auto lit = parseLogicLiteralToken();
                    initialValue = lit->value;
                }
                expectValue(";");

                std::vector<std::unique_ptr<SignalDeclaration>> result;
                for (auto& n : names)
                {
                    m_symbols[n] = SignalInfo{ spec.width, spec.isDownto, spec.low, spec.high, spec.isVector };

                    auto decl = std::make_unique<SignalDeclaration>();
                    decl->signalName = n;
                    decl->width = spec.width;
                    decl->initialValue = initialValue;
                    result.push_back(std::move(decl));
                }
                return result;
            }

            // ---------------------------------------------------------------
            // Component instantiation
            // ---------------------------------------------------------------

            std::unique_ptr<ComponentInstantiation> parseInstantiation()
            {
                std::string instName = expectIdentifier().value;
                expectValue(":");

                if (peek().value == "entity") next(); // "entity work.foo" form - library prefix ignored

                std::string compName = expectIdentifier().value;
                if (peek().value == ".") // library.component form
                {
                    next();
                    compName = expectIdentifier().value;
                }

                expectValue("port");
                expectValue("map");
                expectValue("(");

                auto inst = std::make_unique<ComponentInstantiation>();
                inst->instanceName = instName;
                inst->componentName = compName;

                while (true)
                {
                    std::string portName = expectIdentifier().value;
                    expectValue("=>");
                    auto actual = parseSignalReference();
                    inst->portMaps[portName] = std::move(actual);

                    if (peek().value == ",") { next(); continue; }
                    break;
                }
                expectValue(")");
                expectValue(";");
                return inst;
            }

            // ---------------------------------------------------------------
            // Signal assignment ( target <= value [when cond else value ...]; )
            // ---------------------------------------------------------------

            std::unique_ptr<SignalAssignment> parseSignalAssignment()
            {
                auto target = parseSignalReference();
                expectValue("<=");
                auto value = parseAssignmentValue();
                expectValue(";");

                auto stmt = std::make_unique<SignalAssignment>();
                stmt->target = std::move(target);
                stmt->value = std::move(value);
                return stmt;
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseAssignmentValue()
            {
                auto first = parseValueExpr();

                if (peek().value != "when")
                    return first;
                next();

                auto whenExpr = std::make_unique<WhenElseExpr>();
                auto cond = parseCondition();
                whenExpr->branches.push_back(WhenElseExpr::Branch{ std::move(first), std::move(cond) });

                while (true)
                {
                    expectValue("else");
                    auto val = parseValueExpr();
                    if (peek().value == "when")
                    {
                        next();
                        auto c = parseCondition();
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

            // ---------------------------------------------------------------
            // Signal references, with flattened bit/range indexing
            // ---------------------------------------------------------------

            static std::unique_ptr<IntegerLiteralExpr> makeIntLit(int64_t v)
            {
                auto n = std::make_unique<IntegerLiteralExpr>();
                n->value = v;
                return n;
            }

            // Converts a VHDL-level index into this signal's declaration into a
            // flattened internal position, where internal position 0 always
            // corresponds to the bit written SECOND in the declaration's range
            // (i.e. the rightmost element), regardless of whether it was
            // declared with DOWNTO or TO.
            static int64_t flattenIndex(const SignalInfo& info, int64_t i)
            {
                if (!info.isVector)
                    return 0;
                return info.isDownto ? (i - info.declaredLow) : (info.declaredHigh - i);
            }

            std::unique_ptr<SignalReference> parseSignalReference()
            {
                Token nameTok = expectIdentifier();
                auto it = m_symbols.find(nameTok.value);
                if (it == m_symbols.end())
                    error("undeclared signal '" + nameTok.value + "'");
                const SignalInfo& info = it->second;

                auto ref = std::make_unique<SignalReference>();
                ref->signalName = nameTok.value;

                if (peek().value == "(")
                {
                    next();
                    int64_t firstIdx = parseConstIntExpr();

                    bool isDownto = peek().value == "downto";
                    bool isTo = peek().value == "to";

                    if (isDownto || isTo)
                    {
                        next();
                        int64_t secondIdx = parseConstIntExpr();

                        if (isDownto && firstIdx < secondIdx)
                            error("vector declared with DOWNTO but first bound is less than second. Found: (" + std::to_string(firstIdx) + " downto " + std::to_string(secondIdx) + ")");
                        if (isTo && firstIdx > secondIdx)
                            error("vector declared with TO but first bound is greater than second. Found: (" + std::to_string(firstIdx) + " to " + std::to_string(secondIdx) + ")");

                        expectValue(")");
                        ref->high = makeIntLit(flattenIndex(info, firstIdx));
                        ref->low = makeIntLit(flattenIndex(info, secondIdx));
                    }
                    else
                    {
                        expectValue(")");
                        ref->low = makeIntLit(flattenIndex(info, firstIdx));
                    }
                }

                return ref;
            }

            // ---------------------------------------------------------------
            // Constant integer expressions (range bounds, indices) - folded
            // immediately since flattening needs concrete numbers.
            // ---------------------------------------------------------------

            int64_t parseConstIntExpr()
            {
                int64_t v = parseConstMul();
                while (peek().value == "+" || peek().value == "-")
                {
                    bool plus = (next().value == "+");
                    int64_t r = parseConstMul();
                    v = plus ? v + r : v - r;
                }
                return v;
            }

            int64_t parseConstMul()
            {
                int64_t v = parseConstFactor();
                while (peek().value == "*")
                {
                    next();
                    v *= parseConstFactor();
                }
                return v;
            }

            int64_t parseConstFactor()
            {
                if (peek().value == "-") { next(); return -parseConstFactor(); }
                if (peek().value == "+") { next(); return parseConstFactor(); }
                if (peek().value == "(")
                {
                    next();
                    int64_t v = parseConstIntExpr();
                    expectValue(")");
                    return v;
                }

                Token t = next();
                if (t.type == TokenType::NumericLiteral)
                    return parseIntegerLiteralText(t.value);

                if (t.type == TokenType::Identifier)
                {
                    if (peek().value == "'") next(); // tokenizer may emit the tick separately
                    Token attrTok = next();
                    if (attrTok.type != TokenType::Attribute)
                        error("expected attribute after '" + t.value + "'");
                    return evalConstAttribute(t.value, attrTok.value);
                }

                error("expected integer constant, got '" + t.value + "'");
            }

            int64_t evalConstAttribute(const std::string& signalName, std::string attr)
            {
                if (!attr.empty() && attr.front() == '\'') attr.erase(0, 1);
                attr = toLower(attr);

                auto it = m_symbols.find(signalName);
                if (it == m_symbols.end())
                    error("unknown signal in attribute expression: '" + signalName + "'");
                const SignalInfo& info = it->second;

                if (attr == "length") return info.width;
                if (!info.isVector)
                    error("'" + attr + " is not valid on scalar signal '" + signalName + "'");

                if (attr == "high") return info.declaredHigh;
                if (attr == "low") return info.declaredLow;
                if (attr == "left") return info.isDownto ? info.declaredHigh : info.declaredLow;
                if (attr == "right") return info.isDownto ? info.declaredLow : info.declaredHigh;

                error("unsupported attribute '" + attr + "'");
            }

            // ---------------------------------------------------------------
            // Integer sub-expressions kept as AST (shift amounts, etc.) -
            // same grammar shape as parseConstIntExpr but not folded.
            // ---------------------------------------------------------------

            std::unique_ptr<ASTNode> parseIntegerExpr()
            {
                auto v = parseIntMul();
                while (peek().value == "+" || peek().value == "-")
                {
                    std::string op = next().value;
                    auto r = parseIntMul();
                    auto node = std::make_unique<BinaryOpExpr<ReturnType::INTEGER>>();
                    node->op = op;
                    node->left = std::move(v);
                    node->right = std::move(r);
                    v = std::move(node);
                }
                return v;
            }

            std::unique_ptr<ASTNode> parseIntMul()
            {
                auto v = parseIntFactor();
                while (peek().value == "*")
                {
                    next();
                    auto r = parseIntFactor();
                    auto node = std::make_unique<BinaryOpExpr<ReturnType::INTEGER>>();
                    node->op = "*";
                    node->left = std::move(v);
                    node->right = std::move(r);
                    v = std::move(node);
                }
                return v;
            }

            std::unique_ptr<ASTNode> parseIntFactor()
            {
                if (peek().value == "-")
                {
                    next();
                    auto operand = parseIntFactor();
                    auto node = std::make_unique<UnaryOpExpr<ReturnType::INTEGER>>();
                    node->op = "-";
                    node->operand = std::move(operand);
                    return node;
                }
                if (peek().value == "(")
                {
                    next();
                    auto e = parseIntegerExpr();
                    expectValue(")");
                    return e;
                }

                Token t = next();
                if (t.type == TokenType::NumericLiteral)
                {
                    auto n = std::make_unique<IntegerLiteralExpr>();
                    n->value = parseIntegerLiteralText(t.value);
                    return n;
                }

                if (t.type == TokenType::Identifier)
                {
                    if (peek().value == "'") next();
                    Token attrTok = next();
                    if (attrTok.type != TokenType::Attribute)
                        error("expected attribute after '" + t.value + "'");

                    auto n = std::make_unique<AttributeExpr>();
                    n->kind = attributeKindFromString(attrTok.value);
                    n->target = std::make_unique<SignalReference>();
                    n->target->signalName = t.value;
                    return n;
                }

                error("expected integer expression, got '" + t.value + "'");
            }

            static AttributeKind attributeKindFromString(std::string s)
            {
                if (!s.empty() && s.front() == '\'') s.erase(0, 1);
                s = toLower(s);
                if (s == "left") return AttributeKind::Left;
                if (s == "right") return AttributeKind::Right;
                if (s == "low") return AttributeKind::Low;
                if (s == "high") return AttributeKind::High;
                if (s == "length") return AttributeKind::Length;
                throw std::runtime_error("unsupported attribute '" + s + "'");
            }

            // ---------------------------------------------------------------
            // LOGIC-valued expressions (signal assignment RHS)
            //
            // Precedence, low to high:
            //   logical ops (and/or/nand/nor/xor/xnor)
            //   shift/rotate ops (sll/srl/sla/sra/rol/ror)
            //   adding ops (+ - &)
            //   unary (+ - not)
            //   multiplying (*)
            //   primary (literal, signal ref, signed()/unsigned(), parens)
            // ---------------------------------------------------------------

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseValueExpr()
            {
                return parseLogicalLevel();
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseLogicalLevel()
            {
                static const char* ops[] = { "and", "or", "nand", "nor", "xor", "xnor" };
                auto left = parseShiftLevel();
                while (matchesAny(peek().value, ops, 6))
                {
                    std::string op = toLower(next().value);
                    auto right = parseShiftLevel();
                    left = wrapBinary(op, std::move(left), std::move(right));
                }
                return left;
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseShiftLevel()
            {
                static const char* ops[] = { "sll", "srl", "sla", "sra", "rol", "ror" };
                auto left = parseAddingLevel();
                while (matchesAny(peek().value, ops, 6))
                {
                    std::string op = toLower(next().value);
                    auto amount = parseIntegerExpr();
                    auto node = std::make_unique<BinaryOpExpr<ReturnType::LOGIC>>();
                    node->op = op;
                    node->left = std::move(left);
                    node->right = std::move(amount);
                    left = std::move(node);
                }
                return left;
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseAddingLevel()
            {
                auto left = parseUnaryLevel();
                while (peek().value == "+" || peek().value == "-" || peek().value == "&")
                {
                    std::string op = next().value;
                    auto right = parseUnaryLevel();
                    left = wrapBinary(op, std::move(left), std::move(right));
                }
                return left;
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseUnaryLevel()
            {
                if (peek().value == "not")
                {
                    next();
                    auto operand = parseUnaryLevel();
                    auto node = std::make_unique<UnaryOpExpr<ReturnType::LOGIC>>();
                    node->op = "not";
                    node->operand = std::move(operand);
                    return node;
                }
                if (peek().value == "+" || peek().value == "-")
                {
                    std::string op = next().value;
                    auto operand = parseUnaryLevel();
                    auto node = std::make_unique<UnaryOpExpr<ReturnType::LOGIC>>();
                    node->op = op;
                    node->operand = std::move(operand);
                    return node;
                }
                return parseMultLevel();
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parseMultLevel()
            {
                auto left = parsePrimary();
                while (peek().value == "*")
                {
                    next();
                    auto right = parsePrimary();
                    left = wrapBinary("*", std::move(left), std::move(right));
                }
                return left;
            }

            std::unique_ptr<Expression<ReturnType::LOGIC>> parsePrimary()
            {
                const Token& t = peek();

                if (t.value == "(")
                {
                    next();
                    auto e = parseValueExpr();
                    expectValue(")");
                    return e;
                }

                if (t.value == "signed" || t.value == "unsigned")
                {
                    std::string fname = toLower(next().value);
                    expectValue("(");
                    auto arg = parseValueExpr();
                    expectValue(")");

                    auto node = std::make_unique<FunctionCallExpr<ReturnType::LOGIC>>();
                    node->functionName = fname;
                    node->arguments.push_back(std::move(arg));
                    return node;
                }

                if (t.type == TokenType::BitStringLiteral || t.type == TokenType::CharacterLiteral)
                    return parseLogicLiteralToken();

                if (t.type == TokenType::Identifier)
                    return parseSignalReference();

                error("unexpected token in expression: '" + t.value + "'");
            }

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

            static bool matchesAny(const std::string& v, const char* const* opts, size_t n)
            {
                for (size_t i = 0; i < n; ++i)
                    if (v == opts[i]) return true;
                return false;
            }

            // ---------------------------------------------------------------
            // BOOLEAN-valued expressions (when/else conditions)
            // ---------------------------------------------------------------

            std::unique_ptr<Expression<ReturnType::BOOLEAN>> parseCondition()
            {
                static const char* ops[] = { "and", "or", "nand", "nor", "xor", "xnor" };
                auto left = parseBoolTerm();
                while (matchesAny(peek().value, ops, 6))
                {
                    std::string op = toLower(next().value);
                    auto right = parseBoolTerm();
                    auto node = std::make_unique<BinaryOpExpr<ReturnType::BOOLEAN>>();
                    node->op = op;
                    node->left = std::move(left);
                    node->right = std::move(right);
                    left = std::move(node);
                }
                return left;
            }

            std::unique_ptr<Expression<ReturnType::BOOLEAN>> parseBoolTerm()
            {
                if (peek().value == "not")
                {
                    next();
                    auto operand = parseBoolTerm();
                    auto node = std::make_unique<UnaryOpExpr<ReturnType::BOOLEAN>>();
                    node->op = "not";
                    node->operand = std::move(operand);
                    return node;
                }
                if (peek().value == "(")
                {
                    next();
                    auto e = parseCondition();
                    expectValue(")");
                    return e;
                }

                auto left = parseValueExpr();

                static const char* relops[] = { "=", "/=", "<", "<=", ">", ">=" };
                if (matchesAny(peek().value, relops, 6))
                {
                    std::string op = next().value;
                    auto right = parseValueExpr();
                    auto node = std::make_unique<BinaryOpExpr<ReturnType::BOOLEAN>>();
                    node->op = op;
                    node->left = std::move(left);
                    node->right = std::move(right);
                    return node;
                }

                error("expected relational operator in condition");
            }

            // ---------------------------------------------------------------
            // Literals
            // ---------------------------------------------------------------

            std::unique_ptr<LogicLiteralExpr> parseLogicLiteralToken()
            {
                Token t = next();
                if (t.type == TokenType::BitStringLiteral) return bitStringToLiteral(t.value);
                if (t.type == TokenType::CharacterLiteral) return charToLiteral(t.value);
                error("expected a STD_LOGIC literal, got '" + t.value + "'");
            }

            std::unique_ptr<LogicLiteralExpr> bitStringToLiteral(const std::string& raw)
            {
                size_t i = 0;
                char radix = 'b';

                if (i < raw.size() && std::isalpha(static_cast<unsigned char>(raw[i])))
                {
                    radix = static_cast<char>(std::tolower(static_cast<unsigned char>(raw[i])));
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
                    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

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

                if (width > 64) error("bit string literal wider than 64 bits is not supported");

                auto lit = std::make_unique<LogicLiteralExpr>();
                lit->value = value;
                lit->unknownMask = mask;
                lit->width = static_cast<Pulse::bitWidth_t>(width);
                return lit;
            }

            std::unique_ptr<LogicLiteralExpr> charToLiteral(const std::string& raw)
            {
                char c;
                if (raw.size() >= 3 && raw.front() == '\'' && raw.back() == '\'') c = raw[1];
                else if (!raw.empty()) c = raw[0];
                else error("empty character literal");

                char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                auto lit = std::make_unique<LogicLiteralExpr>();
                lit->width = 1;
                if (lc == '0') { lit->value = 0; lit->unknownMask = 0; }
                else if (lc == '1') { lit->value = 1; lit->unknownMask = 0; }
                else { lit->value = 0; lit->unknownMask = 1; } // X/Z/U/W/-/L/H -> unknown
                return lit;
            }

            // ---------------------------------------------------------------

            TokenStream m_stream;
            std::unordered_map<std::string, SignalInfo> m_symbols;
            std::unordered_map<std::string, std::unordered_map<std::string, SignalInfo>> m_entityPortInfo;
        };
    } // namespace

    ASTBuilder::ASTBuilder(Tokenizer& tokenizer)
    {
        Parser parser(tokenizer);
        root = parser.parse();
    }

    RootNode& ASTBuilder::getRoot()
    {
        return root;
    }
} // namespace Pulse::Parser::VHDL