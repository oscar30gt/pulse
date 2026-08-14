#include <gtest/gtest.h>
#include <vector>

#include "miniSet.h"

TEST(MiniSetTest, IsEmptyInitially) {
    MiniSet<int*> set;
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
}

TEST(MiniSetTest, InsertNullptrIsIgnored) {
    MiniSet<int*> set;
    EXPECT_FALSE(set.insert(nullptr));
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0);
}

TEST(MiniSetTest, InsertSingleElement) {
    MiniSet<int*> set;
    int a = 1;
    
    EXPECT_TRUE(set.insert(&a));
    EXPECT_FALSE(set.empty());
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(set.contains(&a));
}

TEST(MiniSetTest, InsertDuplicateIsIgnored) {
    MiniSet<int*> set;
    int a = 1;
    
    EXPECT_TRUE(set.insert(&a));
    EXPECT_FALSE(set.insert(&a));
    
    EXPECT_EQ(set.size(), 1);
}

TEST(MiniSetTest, InsertMultipleElements) {
    MiniSet<int*> set;
    int a = 1, b = 2, c = 3;
    
    EXPECT_TRUE(set.insert(&a));
    EXPECT_TRUE(set.insert(&b));
    EXPECT_TRUE(set.insert(&c));
    
    EXPECT_EQ(set.size(), 3);
    EXPECT_TRUE(set.contains(&a));
    EXPECT_TRUE(set.contains(&b));
    EXPECT_TRUE(set.contains(&c));
}

TEST(MiniSetTest, EraseElement) {
    MiniSet<int*> set;
    int a = 1, b = 2;
    
    set.insert(&a);
    set.insert(&b);
    
    // Erase existing
    EXPECT_TRUE(set.erase(&a));
    EXPECT_EQ(set.size(), 1);
    EXPECT_FALSE(set.contains(&a));
    EXPECT_TRUE(set.contains(&b));
    
    // Erase non-existing
    int c = 3;
    EXPECT_FALSE(set.erase(&c));
    
    // Erase remaining
    EXPECT_TRUE(set.erase(&b));
    EXPECT_TRUE(set.empty());
}

TEST(MiniSetTest, Iteration) {
    MiniSet<int*> set;
    int a = 1, b = 2, c = 3;
    
    set.insert(&a);
    set.insert(&b);
    set.insert(&c);
    
    std::vector<int*> collected;
    for (int* ptr : set) {
        collected.push_back(ptr);
    }
    
    EXPECT_EQ(collected.size(), 3);
    
    // Iteration order is not guaranteed, so we just check for presence
    bool has_a = false, has_b = false, has_c = false;
    for (int* ptr : collected) {
        if (ptr == &a) has_a = true;
        if (ptr == &b) has_b = true;
        if (ptr == &c) has_c = true;
    }
    
    EXPECT_TRUE(has_a);
    EXPECT_TRUE(has_b);
    EXPECT_TRUE(has_c);
}

TEST(MiniSetTest, CopyConstruction) {
    MiniSet<int*> set1;
    int a = 1, b = 2;
    set1.insert(&a);
    set1.insert(&b);
    
    MiniSet<int*> set2 = set1; // Copy
    EXPECT_EQ(set2.size(), 2);
    EXPECT_TRUE(set2.contains(&a));
    EXPECT_TRUE(set2.contains(&b));
    
    // Modifying set2 shouldn't affect set1
    set2.erase(&a);
    EXPECT_TRUE(set1.contains(&a));
    EXPECT_FALSE(set2.contains(&a));
}

TEST(MiniSetTest, MoveConstruction) {
    MiniSet<int*> set1;
    int a = 1, b = 2;
    set1.insert(&a);
    set1.insert(&b);
    
    MiniSet<int*> set2 = std::move(set1);
    EXPECT_EQ(set2.size(), 2);
    EXPECT_TRUE(set2.contains(&a));
    EXPECT_TRUE(set2.contains(&b));
    
    // set1 should be in a valid but empty state
    EXPECT_TRUE(set1.empty());
}

TEST(MiniSetTest, CopyAssignment) {
    MiniSet<int*> set1;
    int a = 1, b = 2;
    set1.insert(&a);
    set1.insert(&b);
    
    MiniSet<int*> set2;
    int c = 3;
    set2.insert(&c);
    
    set2 = set1;
    EXPECT_EQ(set2.size(), 2);
    EXPECT_TRUE(set2.contains(&a));
    EXPECT_TRUE(set2.contains(&b));
    EXPECT_FALSE(set2.contains(&c));
}

TEST(MiniSetTest, MoveAssignment) {
    MiniSet<int*> set1;
    int a = 1, b = 2;
    set1.insert(&a);
    set1.insert(&b);
    
    MiniSet<int*> set2;
    int c = 3;
    set2.insert(&c);
    
    set2 = std::move(set1);
    EXPECT_EQ(set2.size(), 2);
    EXPECT_TRUE(set2.contains(&a));
    EXPECT_TRUE(set2.contains(&b));
    EXPECT_FALSE(set2.contains(&c));
    EXPECT_TRUE(set1.empty());
}
