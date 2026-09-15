#include "normalizer.h"
#include <cmath>
#include <cstdlib>

namespace Pulse::Parser
{
    namespace 
    {
        class ASTNormalizerVisitor 
        {
        public:
            void normalize(ASTRoot& root) 
            {
                for (auto& child : root.children) 
                {
                    visitNode(child);
                }
            }

        private:
            void visitNode(std::unique_ptr<ASTNode>& node) 
            {
                if (!node) return;
                
                if (auto entity = dynamic_cast<EntityDeclaration*>(node.get())) 
                {
                    for (auto& port : entity->ports) visitPort(port);
                } 
                else if (auto arch = dynamic_cast<ArchitectureDeclaration*>(node.get())) 
                {
                    for (auto& sig : arch->signals) visitSignal(sig);
                    
                    for (auto& comp : arch->components) 
                    {
                        for (auto& port : comp.ports) visitPort(port);
                    }
                    
                    for (auto& stmt : arch->body) visitStatement(stmt);
                }
            }

            void visitPort(PortDeclaration& port) 
            {
                normalizeTypeSpec(port.typeSpec);
            }

            void visitSignal(SignalDeclaration& sig) 
            {
                normalizeTypeSpec(sig.typeSpec);
                if (sig.initialValue) 
                {
                    visitExpression(sig.initialValue);
                }
            }

            void visitStatement(std::unique_ptr<Statement>& stmt) 
            {
                if (!stmt) return;
                
                if (auto sa = dynamic_cast<SignalAssignment*>(stmt.get())) 
                {
                    visitExpression(sa->target);
                    visitExpression(sa->value);
                } 
                else if (auto ps = dynamic_cast<ProcessStatement*>(stmt.get())) 
                {
                    for (auto& s : ps->body) visitStatement(s);
                } 
                else if (auto is = dynamic_cast<IfStatement*>(stmt.get())) 
                {
                    for (auto& branch : is->branches) 
                    {
                        visitExpression(branch.condition);
                        for (auto& s : branch.body) visitStatement(s);
                    }
                    for (auto& s : is->elseBody) visitStatement(s);
                } 
                else if (auto wc = dynamic_cast<WithClause*>(stmt.get())) 
                {
                    // Structurally, WithClause is a Statement without a target in ast.h.
                    // This makes it impossible to wrap nested WhenElseExpr inside a valid 
                    // SignalAssignment. We merely traverse it to normalize interior expressions.
                    visitExpression(wc->selector);
                    for (auto& choice : wc->choices) 
                    {
                        visitExpression(choice.first);
                        visitExpression(choice.second);
                    }
                    if (wc->defaultValue) 
                    {
                        visitExpression(wc->defaultValue);
                    }
                } 
                else if (auto ci = dynamic_cast<ComponentInstantiation*>(stmt.get())) 
                {
                    // portMap contains pairs of string and SymbolExpr. 
                    // SymbolExpr has no children to normalize.
                }
            }

            void visitExpression(std::unique_ptr<Expression>& expr) 
            {
                if (!expr) return;
                
                // Bottom-up traversal first
                if (auto binOp = dynamic_cast<BinaryOpExpr*>(expr.get())) 
                {
                    visitExpression(binOp->left);
                    visitExpression(binOp->right);
                } 
                else if (auto unOp = dynamic_cast<UnaryOpExpr*>(expr.get())) 
                {
                    visitExpression(unOp->operand);
                } 
                else if (auto fc = dynamic_cast<FunctionCallExpr*>(expr.get())) 
                {
                    for (auto& arg : fc->arguments) visitExpression(arg);
                } 
                else if (auto we = dynamic_cast<WhenElseExpr*>(expr.get())) 
                {
                    visitExpression(we->trueValue);
                    visitExpression(we->condition);
                    if (we->falseValue) visitExpression(we->falseValue);
                }

                // Apply literal replacements dynamically post-traversal
                if (auto boolLit = dynamic_cast<BooleanLiteral*>(expr.get())) 
                {
                    auto logic = std::make_unique<LogicLiteralExpr>();
                    logic->typeName = "std_logic";
                    logic->value = boolLit->value ? 1 : 0;
                    logic->mask = 0;
                    logic->width = 1;
                    logic->source = boolLit->source;
                    expr = std::move(logic); 
                } 
                else if (auto intLit = dynamic_cast<IntegerLiteralExpr*>(expr.get())) 
                {
                    auto logic = std::make_unique<LogicLiteralExpr>();
                    logic->typeName = "std_logic_vector";
                    logic->value = intLit->value;
                    logic->mask = 0;
                    logic->width = 32;
                    logic->source = intLit->source;
                    expr = std::move(logic); 
                }
            }

            void normalizeTypeSpec(TypeSpec& ts) 
            {
                // 1. Normalize ranges: e.g. (X downto Y) or (Y to X) -> ((X-Y) downto 0)
                if (ts.args.size() == 1) 
                {
                    if (auto binOp = dynamic_cast<BinaryOpExpr*>(ts.args[0].get())) 
                    {
                        if (binOp->op == "downto" || binOp->op == "to") 
                        {
                            auto lInt = dynamic_cast<IntegerLiteralExpr*>(binOp->left.get());
                            auto rInt = dynamic_cast<IntegerLiteralExpr*>(binOp->right.get());
                            
                            if (lInt && rInt) 
                            {
                                int64_t w = std::abs(lInt->value - rInt->value) + 1;
                                lInt->value = w - 1;
                                rInt->value = 0;
                                binOp->op = "downto";
                            }
                        }
                    }
                }
            }
        };
    }

    void normalizeAST(ASTRoot& root)
    {
        ASTNormalizerVisitor visitor;
        visitor.normalize(root);
    }
    
} // namespace Pulse::Parser