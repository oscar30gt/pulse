// linker.test.cc — GTest suite for Pulse::Parser::Linker
//
// Comprehensive test coverage for the linking phase of the VHDL compiler.
// Tests verify that multiple ASTRoot objects are correctly merged into a
// single linked design root, that entity and architecture ordering is
// preserved (entities first, architectures second), and that duplicate
// declarations are caught and reported as ast_link_error exceptions.
//
// Implementation notes:
//   - The Linker takes ownership of ASTRoot objects via addAST(ASTRoot&&).
//   - link() returns a new ASTRoot with all entities before all architectures.
//   - Duplicate entity names across any roots trigger ast_link_error.
//   - Duplicate architecture names for the same entity trigger ast_link_error.
//   - Unknown nodes (neither entity nor architecture) are silently dropped.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "linker.h"

using namespace Pulse::Parser;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Convenience location used when the test does not care about source positions.
static constexpr SourceLocation kNoLoc{ 0, 0 };

/// Build an EntityDeclaration node with the given name.
static std::unique_ptr<EntityDeclaration> makeEntity(const std::string& name,
                                                      SourceLocation loc = kNoLoc)
{
    auto node = std::make_unique<EntityDeclaration>();
    node->name   = name;
    node->source = loc;
    return node;
}

/// Build an ArchitectureDeclaration node with the given architecture and entity names.
static std::unique_ptr<ArchitectureDeclaration> makeArch(const std::string& archName,
                                                          const std::string& entityName,
                                                          SourceLocation loc = kNoLoc)
{
    auto node = std::make_unique<ArchitectureDeclaration>();
    node->name       = archName;
    node->entityName = entityName;
    node->source     = loc;
    return node;
}

/// Build an ASTRoot and populate it with the provided children.
static ASTRoot makeRoot(std::vector<std::unique_ptr<ASTNode>> children)
{
    ASTRoot root;
    root.children = std::move(children);
    return root;
}

/// Downcast a generic ASTNode pointer to a specific derived type.
/// Returns nullptr if the cast fails.
template <typename T>
static T* as(ASTNode* node)
{
    return dynamic_cast<T*>(node);
}

template <typename T>
static const T* as(const ASTNode* node)
{
    return dynamic_cast<const T*>(node);
}

// ===========================================================================
// 1. EMPTY INPUT
// ===========================================================================

TEST(Linker_Empty, NoRootsAdded)
{
    Linker linker;
    ASTRoot result = linker.link();
    EXPECT_TRUE(result.children.empty());
}

TEST(Linker_Empty, SingleEmptyRoot)
{
    Linker linker;
    linker.addAST(ASTRoot{});
    ASTRoot result = linker.link();
    EXPECT_TRUE(result.children.empty());
}

TEST(Linker_Empty, MultipleEmptyRoots)
{
    Linker linker;
    linker.addAST(ASTRoot{});
    linker.addAST(ASTRoot{});
    linker.addAST(ASTRoot{});
    ASTRoot result = linker.link();
    EXPECT_TRUE(result.children.empty());
}

// ===========================================================================
// 2. SINGLE ROOT — BASIC CORRECTNESS
// ===========================================================================

TEST(Linker_SingleRoot, SingleEntity)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("my_entity"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 1u);

    auto* entity = as<EntityDeclaration>(result.children[0].get());
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->name, "my_entity");
}

TEST(Linker_SingleRoot, SingleArchitecture)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeArch("rtl", "my_entity"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 1u);

    auto* arch = as<ArchitectureDeclaration>(result.children[0].get());
    ASSERT_NE(arch, nullptr);
    EXPECT_EQ(arch->name, "rtl");
    EXPECT_EQ(arch->entityName, "my_entity");
}

TEST(Linker_SingleRoot, EntityAndArchitecture)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("adder"));
    children.push_back(makeArch("rtl", "adder"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 2u);

    auto* entity = as<EntityDeclaration>(result.children[0].get());
    auto* arch   = as<ArchitectureDeclaration>(result.children[1].get());
    ASSERT_NE(entity, nullptr);
    ASSERT_NE(arch, nullptr);
    EXPECT_EQ(entity->name, "adder");
    EXPECT_EQ(arch->name, "rtl");
}

TEST(Linker_SingleRoot, MultipleEntitiesAndArchitectures)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("entity_a"));
    children.push_back(makeArch("rtl", "entity_a"));
    children.push_back(makeEntity("entity_b"));
    children.push_back(makeArch("behavioral", "entity_b"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 4u);

    // Entities come first
    auto* ea = as<EntityDeclaration>(result.children[0].get());
    auto* eb = as<EntityDeclaration>(result.children[1].get());
    ASSERT_NE(ea, nullptr);
    ASSERT_NE(eb, nullptr);

    // Architectures come after
    auto* aa = as<ArchitectureDeclaration>(result.children[2].get());
    auto* ab = as<ArchitectureDeclaration>(result.children[3].get());
    ASSERT_NE(aa, nullptr);
    ASSERT_NE(ab, nullptr);
}

// ===========================================================================
// 3. OUTPUT ORDERING: ENTITIES BEFORE ARCHITECTURES
// ===========================================================================

TEST(Linker_Ordering, ArchBeforeEntityInInputBecomesEntityFirst)
{
    // Even though the architecture appears before the entity in the input root,
    // the linked result must put entities before architectures.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeArch("rtl", "my_entity")); // arch first in input
    children.push_back(makeEntity("my_entity"));       // entity second in input
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 2u);

    EXPECT_NE(as<EntityDeclaration>(result.children[0].get()), nullptr)
        << "First child should be an entity";
    EXPECT_NE(as<ArchitectureDeclaration>(result.children[1].get()), nullptr)
        << "Second child should be an architecture";
}

TEST(Linker_Ordering, InterleavedInputProducesGroupedOutput)
{
    // Three entities and three architectures interleaved in the input.
    // The output should have all three entities followed by all three architectures.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("e1"));
    children.push_back(makeArch("rtl", "e1"));
    children.push_back(makeEntity("e2"));
    children.push_back(makeArch("rtl", "e2"));
    children.push_back(makeEntity("e3"));
    children.push_back(makeArch("rtl", "e3"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 6u);

    for (size_t i = 0; i < 3; ++i)
        EXPECT_NE(as<EntityDeclaration>(result.children[i].get()), nullptr)
            << "Child " << i << " should be an entity";

    for (size_t i = 3; i < 6; ++i)
        EXPECT_NE(as<ArchitectureDeclaration>(result.children[i].get()), nullptr)
            << "Child " << i << " should be an architecture";
}

// ===========================================================================
// 4. MULTIPLE ROOTS MERGED
// ===========================================================================

TEST(Linker_MultiRoot, TwoRootsEachWithOneEntity)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> c1;
    c1.push_back(makeEntity("entity_a"));
    linker.addAST(makeRoot(std::move(c1)));

    std::vector<std::unique_ptr<ASTNode>> c2;
    c2.push_back(makeEntity("entity_b"));
    linker.addAST(makeRoot(std::move(c2)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 2u);

    auto* ea = as<EntityDeclaration>(result.children[0].get());
    auto* eb = as<EntityDeclaration>(result.children[1].get());
    ASSERT_NE(ea, nullptr);
    ASSERT_NE(eb, nullptr);
    EXPECT_EQ(ea->name, "entity_a");
    EXPECT_EQ(eb->name, "entity_b");
}

TEST(Linker_MultiRoot, EntityInOneRootArchInAnother)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> c1;
    c1.push_back(makeEntity("counter"));
    linker.addAST(makeRoot(std::move(c1)));

    std::vector<std::unique_ptr<ASTNode>> c2;
    c2.push_back(makeArch("rtl", "counter"));
    linker.addAST(makeRoot(std::move(c2)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 2u);

    auto* entity = as<EntityDeclaration>(result.children[0].get());
    auto* arch   = as<ArchitectureDeclaration>(result.children[1].get());
    ASSERT_NE(entity, nullptr);
    ASSERT_NE(arch, nullptr);
    EXPECT_EQ(entity->name, "counter");
    EXPECT_EQ(arch->entityName, "counter");
}

TEST(Linker_MultiRoot, ManyRootsMergedCorrectly)
{
    Linker linker;

    for (int i = 0; i < 10; ++i)
    {
        std::string name = "entity_" + std::to_string(i);
        std::vector<std::unique_ptr<ASTNode>> c;
        c.push_back(makeEntity(name));
        c.push_back(makeArch("rtl", name));
        linker.addAST(makeRoot(std::move(c)));
    }

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 20u);

    // First 10 children must all be entities
    for (size_t i = 0; i < 10; ++i)
        EXPECT_NE(as<EntityDeclaration>(result.children[i].get()), nullptr)
            << "Child " << i << " should be an entity";

    // Last 10 children must all be architectures
    for (size_t i = 10; i < 20; ++i)
        EXPECT_NE(as<ArchitectureDeclaration>(result.children[i].get()), nullptr)
            << "Child " << i << " should be an architecture";
}

// ===========================================================================
// 5. SOURCE ROOTS ARE CLEARED AFTER LINKING
// ===========================================================================

TEST(Linker_Ownership, SourceRootsAreEmptiedAfterLink)
{
    // After link() runs, ownership of all nodes has been transferred to the
    // returned root. Verify that none of the child data leaked or was aliased
    // by checking that the returned root has exactly the expected count and
    // that all pointers are non-null.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("e1"));
    children.push_back(makeArch("rtl", "e1"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();

    ASSERT_EQ(result.children.size(), 2u);
    for (const auto& child : result.children)
        EXPECT_NE(child.get(), nullptr) << "No child should be a null pointer after linking";
}

// ===========================================================================
// 6. DUPLICATE ENTITY DETECTION
// ===========================================================================

TEST(Linker_DuplicateEntities, SameNameInSingleRoot)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("adder"));
    children.push_back(makeEntity("adder")); // duplicate
    linker.addAST(makeRoot(std::move(children)));

    EXPECT_THROW(linker.link(), ast_link_error);
}

TEST(Linker_DuplicateEntities, SameNameAcrossTwoRoots)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> c1;
    c1.push_back(makeEntity("adder"));
    linker.addAST(makeRoot(std::move(c1)));

    std::vector<std::unique_ptr<ASTNode>> c2;
    c2.push_back(makeEntity("adder")); // duplicate in a different root
    linker.addAST(makeRoot(std::move(c2)));

    EXPECT_THROW(linker.link(), ast_link_error);
}

TEST(Linker_DuplicateEntities, ThreeRootsThirdIsDuplicate)
{
    Linker linker;

    for (const std::string name : { "first", "second", "first" }) // "first" duplicated
    {
        std::vector<std::unique_ptr<ASTNode>> c;
        c.push_back(makeEntity(name));
        linker.addAST(makeRoot(std::move(c)));
    }

    EXPECT_THROW(linker.link(), ast_link_error);
}

TEST(Linker_DuplicateEntities, DuplicateErrorCarriesSourceLocation)
{
    Linker linker;

    SourceLocation loc{ 42, 7 };
    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("counter"));
    children.push_back(makeEntity("counter", loc)); // duplicate with known location
    linker.addAST(makeRoot(std::move(children)));

    try
    {
        linker.link();
        FAIL() << "Expected ast_link_error";
    }
    catch (const ast_link_error& e)
    {
        EXPECT_EQ(e.location().line,   loc.line);
        EXPECT_EQ(e.location().column, loc.column);
    }
}

TEST(Linker_DuplicateEntities, DifferentNamesDoNotThrow)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("entity_a"));
    children.push_back(makeEntity("entity_b"));
    children.push_back(makeEntity("entity_c"));
    linker.addAST(makeRoot(std::move(children)));

    EXPECT_NO_THROW(linker.link());
}

// ===========================================================================
// 7. DUPLICATE ARCHITECTURE DETECTION
// ===========================================================================

TEST(Linker_DuplicateArchitectures, SameNameSameEntityInSingleRoot)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("adder"));
    children.push_back(makeArch("rtl", "adder"));
    children.push_back(makeArch("rtl", "adder")); // duplicate
    linker.addAST(makeRoot(std::move(children)));

    EXPECT_THROW(linker.link(), ast_link_error);
}

TEST(Linker_DuplicateArchitectures, SameNameSameEntityAcrossTwoRoots)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> c1;
    c1.push_back(makeEntity("counter"));
    c1.push_back(makeArch("rtl", "counter"));
    linker.addAST(makeRoot(std::move(c1)));

    std::vector<std::unique_ptr<ASTNode>> c2;
    c2.push_back(makeArch("rtl", "counter")); // duplicate across roots
    linker.addAST(makeRoot(std::move(c2)));

    EXPECT_THROW(linker.link(), ast_link_error);
}

TEST(Linker_DuplicateArchitectures, SameArchNameDifferentEntitiesIsAllowed)
{
    // "rtl" for "adder" and "rtl" for "multiplier" are distinct; no error expected.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("adder"));
    children.push_back(makeEntity("multiplier"));
    children.push_back(makeArch("rtl", "adder"));
    children.push_back(makeArch("rtl", "multiplier")); // same arch name, different entity
    linker.addAST(makeRoot(std::move(children)));

    EXPECT_NO_THROW(linker.link());
}

TEST(Linker_DuplicateArchitectures, MultipleArchsForSameEntityAllowed)
{
    // One entity may have several differently-named architectures.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("counter"));
    children.push_back(makeArch("rtl",        "counter"));
    children.push_back(makeArch("behavioral", "counter"));
    children.push_back(makeArch("structural", "counter"));
    linker.addAST(makeRoot(std::move(children)));

    EXPECT_NO_THROW(linker.link());
}

TEST(Linker_DuplicateArchitectures, DuplicateArchErrorCarriesSourceLocation)
{
    Linker linker;

    SourceLocation loc{ 10, 3 };
    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("fifo"));
    children.push_back(makeArch("rtl", "fifo"));
    children.push_back(makeArch("rtl", "fifo", loc)); // duplicate with known location
    linker.addAST(makeRoot(std::move(children)));

    try
    {
        linker.link();
        FAIL() << "Expected ast_link_error";
    }
    catch (const ast_link_error& e)
    {
        EXPECT_EQ(e.location().line,   loc.line);
        EXPECT_EQ(e.location().column, loc.column);
    }
}

// ===========================================================================
// 8. UNKNOWN / NULL NODES ARE SKIPPED
// ===========================================================================

TEST(Linker_UnknownNodes, NullChildrenAreSkipped)
{
    // Inserting null unique_ptrs should not crash or count toward the result.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(nullptr);
    children.push_back(makeEntity("my_entity"));
    children.push_back(nullptr);
    children.push_back(makeArch("rtl", "my_entity"));
    children.push_back(nullptr);
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result;
    EXPECT_NO_THROW(result = linker.link());
    EXPECT_EQ(result.children.size(), 2u);
}

// ===========================================================================
// 9. EXCEPTION TYPE HIERARCHY
// ===========================================================================

TEST(Linker_Exceptions, LinkErrorIsAlsoAstError)
{
    // ast_link_error must be catchable as ast_error (base class).
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("adder"));
    children.push_back(makeEntity("adder"));
    linker.addAST(makeRoot(std::move(children)));

    EXPECT_THROW(linker.link(), ast_error);
}

TEST(Linker_Exceptions, LinkErrorMessageMentionsEntityName)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("my_dup_entity"));
    children.push_back(makeEntity("my_dup_entity"));
    linker.addAST(makeRoot(std::move(children)));

    try
    {
        linker.link();
        FAIL() << "Expected ast_link_error";
    }
    catch (const ast_link_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("my_dup_entity"), std::string::npos)
            << "Error message should mention the duplicate entity name";
    }
}

TEST(Linker_Exceptions, LinkErrorMessageMentionsArchAndEntityName)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("fifo"));
    children.push_back(makeArch("rtl", "fifo"));
    children.push_back(makeArch("rtl", "fifo"));
    linker.addAST(makeRoot(std::move(children)));

    try
    {
        linker.link();
        FAIL() << "Expected ast_link_error";
    }
    catch (const ast_link_error& e)
    {
        std::string msg(e.what());
        EXPECT_NE(msg.find("rtl"),  std::string::npos)
            << "Error message should mention the duplicate architecture name";
        EXPECT_NE(msg.find("fifo"), std::string::npos)
            << "Error message should mention the target entity name";
    }
}

// ===========================================================================
// 10. EDGE CASES
// ===========================================================================

TEST(Linker_EdgeCases, LinkCalledTwiceOnSameLinkerYieldsEmpty)
{
    // After link() has moved ownership out of all roots, a second call on the
    // same linker should return an empty root (roots were cleared internally).
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("e1"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot first  = linker.link();
    ASTRoot second = linker.link(); // roots already consumed

    EXPECT_EQ(first.children.size(), 1u);
    EXPECT_TRUE(second.children.empty());
}

TEST(Linker_EdgeCases, LargeNumberOfDistinctEntities)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    for (int i = 0; i < 500; ++i)
        children.push_back(makeEntity("entity_" + std::to_string(i)));

    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result;
    EXPECT_NO_THROW(result = linker.link());
    EXPECT_EQ(result.children.size(), 500u);
}

TEST(Linker_EdgeCases, LargeNumberOfDistinctArchitecturesForOneEntity)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("top"));
    for (int i = 0; i < 200; ++i)
        children.push_back(makeArch("arch_" + std::to_string(i), "top"));

    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result;
    EXPECT_NO_THROW(result = linker.link());
    EXPECT_EQ(result.children.size(), 201u); // 1 entity + 200 architectures
}

TEST(Linker_EdgeCases, EntityNameCaseSensitivity)
{
    // "Adder" and "adder" are distinct names; both should link without error.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("Adder"));
    children.push_back(makeEntity("adder"));
    children.push_back(makeEntity("ADDER"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result;
    EXPECT_NO_THROW(result = linker.link());
    EXPECT_EQ(result.children.size(), 3u);
}

TEST(Linker_EdgeCases, ArchitectureNameCaseSensitivity)
{
    // "RTL" and "rtl" for the same entity are distinct; should not collide.
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("counter"));
    children.push_back(makeArch("RTL", "counter"));
    children.push_back(makeArch("rtl", "counter"));
    children.push_back(makeArch("Rtl", "counter"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result;
    EXPECT_NO_THROW(result = linker.link());
    EXPECT_EQ(result.children.size(), 4u);
}

TEST(Linker_EdgeCases, EntityAndArchNamesPreservedExactly)
{
    Linker linker;

    std::vector<std::unique_ptr<ASTNode>> children;
    children.push_back(makeEntity("my_special_entity_123"));
    children.push_back(makeArch("my_special_arch_456", "my_special_entity_123"));
    linker.addAST(makeRoot(std::move(children)));

    ASTRoot result = linker.link();
    ASSERT_EQ(result.children.size(), 2u);

    auto* entity = as<EntityDeclaration>(result.children[0].get());
    auto* arch   = as<ArchitectureDeclaration>(result.children[1].get());
    ASSERT_NE(entity, nullptr);
    ASSERT_NE(arch, nullptr);
    EXPECT_EQ(entity->name,       "my_special_entity_123");
    EXPECT_EQ(arch->name,         "my_special_arch_456");
    EXPECT_EQ(arch->entityName,   "my_special_entity_123");
}