// aliases_attributes.test.cc — aliases of objects, types and subprograms, and user-declared attributes: their
// declaration, their specification for every class of item, and reading their values.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kSignals =
        "signal a, b : std_logic; signal n, m : integer; signal p : boolean; "
        "signal v8 : std_logic_vector(7 downto 0); signal v4 : std_logic_vector(3 downto 0); signal u4 : unsigned(3 downto 0); "
        "type color is (red, green, blue); signal col : color; type pair is record x, y : integer; end record; signal pr : pair; ";

    std::string arch(const std::string& declarations, const std::string& body = "")
    {
        return analysisError(inArchitecture(kSignals + declarations, body));
    }

    std::string proc(const std::string& declarations, const std::string& statements, const std::string& processDeclarations = "")
    {
        return arch(declarations, "process " + processDeclarations + " begin " + statements + " wait; end process;");
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

// ===========================================================================
// 1. ALIASES OF OBJECTS
// ===========================================================================

TEST(Aliases_Objects, AnAliasIsAnotherNameForTheObject)
{
    EXPECT_EQ(proc("alias sig : std_logic is a; ", "b <= sig; sig <= '1';"), kOk);
    EXPECT_EQ(proc("alias num : integer is n; ", "m <= num + 1;"), kOk);
    EXPECT_EQ(proc("alias sig is a; ", "b <= sig;"), kOk) << "the subtype indication is optional";
}

TEST(Aliases_Objects, SlicesElementsAndFields)
{
    EXPECT_EQ(proc("alias hi : std_logic_vector(3 downto 0) is v8(7 downto 4); alias lo is v8(3 downto 0); alias top is v8(7); alias fx is pr.x; ",
                   "v4 <= hi and lo; a <= top; n <= fx;"), kOk);
}

TEST(Aliases_Objects, ASubtypeReindexesASlice)
{
    EXPECT_EQ(proc("alias hi : std_logic_vector(0 to 3) is v8(7 downto 4); ", "a <= hi(0); b <= hi(3);"), kOk);
    EXPECT_TRUE(mentions(proc("alias hi : std_logic_vector(0 to 3) is v8(7 downto 4); ", "a <= hi(4);"), "Index 4 is out of bounds"));
}

TEST(Aliases_Objects, TheSubtypeMustMatchWhatItNames)
{
    EXPECT_TRUE(mentions(arch("alias x : integer is a; "), "does not match the type 'std_logic'"));
    const std::string message = arch("alias x : std_logic_vector(3 downto 0) is v8; ");
    EXPECT_TRUE(mentions(message, "has 4 element(s), but 'std_logic_vector(7 downto 0)' has 8")) << message;
    EXPECT_TRUE(mentions(arch("alias x : nothing is a; "), "Unknown type 'nothing'"));
}

TEST(Aliases_Objects, AnUnconstrainedSubtypeKeepsTheBoundsOfTheObject)
{
    EXPECT_EQ(proc("alias x : std_logic_vector is v8; ", "v8 <= x;"), kOk);
    EXPECT_TRUE(mentions(proc("alias x : std_logic_vector is v8; ", "v4 <= x;"), "Length mismatch"));
}

TEST(Aliases_Objects, AnAliasSharesTheModeOfItsTarget)
{
    const std::string entity = "entity t is port (i : in std_logic; o : out std_logic); end t; ";
    EXPECT_EQ(analysisError(entity + "architecture r of t is alias out_a : std_logic is o; begin out_a <= '1'; end r;"), kOk);
    const std::string message = analysisError(entity + "architecture r of t is alias in_a : std_logic is i; begin in_a <= '1'; end r;");
    EXPECT_TRUE(mentions(message, "Port 'in_a' is an input and cannot be assigned")) << message;
}

TEST(Aliases_Objects, AnAliasOfAConstantIsAConstant)
{
    EXPECT_EQ(arch("constant k : integer := 3; alias kk : integer is k; signal s : std_logic_vector(kk downto 0); "), kOk);
    const std::string message = proc("constant k : integer := 3; alias kk : integer is k; ", "kk <= 1;");
    EXPECT_TRUE(mentions(message, "'kk' is a constant and cannot be assigned")) << message;
}

TEST(Aliases_Objects, AnAliasOfAVariableIsAVariable)
{
    EXPECT_EQ(proc("", "al := 4; n <= al;", "variable v : integer := 0; alias al : integer is v;"), kOk);
    EXPECT_TRUE(mentions(proc("", "al <= 4;", "variable v : integer := 0; alias al : integer is v;"), "is a variable; assign it with ':='"));
}

TEST(Aliases_Objects, ADriverThroughAnAliasIsADriverOfTheSignal)
{
    const std::string message = arch("alias other : integer is n; ", "process begin n <= 1; wait; end process; process begin other <= 2; wait; end process;");
    EXPECT_TRUE(mentions(message, "Signal 'n' has the type 'integer', which cannot have several drivers")) << message;
}

TEST(Aliases_Objects, ElementDriversThroughAnAliasStaySeparate)
{
    EXPECT_EQ(arch("type pair2 is array (0 to 1) of integer; signal arr : pair2; alias first : integer is arr(0); alias second : integer is arr(1); ",
                   "process begin first <= 1; wait; end process; process begin second <= 2; wait; end process;"), kOk);
}

TEST(Aliases_Objects, AnAliasOfAnAlias)
{
    EXPECT_EQ(proc("alias one : std_logic is a; alias two : std_logic is one; ", "two <= '1';"), kOk);
}

TEST(Aliases_Objects, TheTargetMustBeDeclared)
{
    EXPECT_TRUE(mentions(arch("alias x : integer is nothing; "), "'nothing' is not declared"));
    EXPECT_TRUE(mentions(arch("alias x is n + 1; "), "Expected ';', but found '+'")) << "an alias names something; it is not an expression";
}

TEST(Aliases_Objects, ANameIsDeclaredOnce)
{
    EXPECT_TRUE(mentions(arch("alias a : std_logic is b; "), "'a' is already declared"));
    EXPECT_TRUE(mentions(arch("alias x : std_logic is a; alias x : std_logic is b; "), "'x' is already declared"));
}

TEST(Aliases_Objects, AnAliasOfAnEnumerationLiteralOrUnitIsAConstant)
{
    EXPECT_EQ(proc("alias r : color is red; ", "col <= r;"), kOk);
    EXPECT_EQ(proc("alias yes is true; ", "p <= yes;"), kOk);
    EXPECT_TRUE(mentions(proc("alias r : color is red; ", "r <= green;"), "constant and cannot be assigned"));
    EXPECT_TRUE(mentions(arch("alias r : integer is red; "), "The subtype 'integer' of the alias does not match the type 'color'"));
}

TEST(Aliases_Objects, AnAliasOfAnEnumerationLiteralMayHaveASignature)
{
    // LRM 6.6.3: a literal is a function without parameters, so its alias may name it with `[return type]`.
    EXPECT_EQ(proc("alias yes is true [return boolean]; ", "p <= yes;"), kOk);
    EXPECT_EQ(proc("alias r is red [return color]; ", "col <= r;"), kOk);
    EXPECT_TRUE(mentions(proc("alias r is red [return color]; ", "r <= green;"), "constant and cannot be assigned"));

    EXPECT_TRUE(mentions(arch("alias r is red [return boolean]; "), "'red' is not a literal of the type 'boolean'"));
    EXPECT_TRUE(mentions(arch("alias r is red [color return color]; "), "must be '[return <type>]'"));
    EXPECT_TRUE(mentions(arch("alias r is red [color]; "), "must be '[return <type>]'"));
    EXPECT_TRUE(mentions(arch("alias r : color is red [return color]; "), "cannot have a subtype indication"));
}

TEST(Aliases_Objects, WhatCannotBeAliasedAsAnObject)
{
    EXPECT_TRUE(mentions(arch("alias x : std_logic is color; "), "An alias of a type cannot have a subtype indication"));
    EXPECT_TRUE(mentions(arch("attribute w : integer; alias x is w; "), "'w' is an attribute, not a value"));
    EXPECT_TRUE(mentions(arch("alias x is main; ", "main : process begin wait; end process;"), "'main' is not declared"))
        << "a label is declared by its statement, after the declarative part";
    EXPECT_TRUE(mentions(arch("function f return integer is begin return 1; end; alias x : integer is f; "), "cannot have a subtype indication"));
}

// ===========================================================================
// 2. ALIASES OF TYPES
// ===========================================================================

TEST(Aliases_Types, ANewNameForAType)
{
    EXPECT_EQ(proc("alias bit_t is std_logic; alias word is std_logic_vector; ", "a <= '1';", "variable v : bit_t; variable w : word(3 downto 0);"), kOk);
    EXPECT_EQ(arch("alias hue is color; signal c2 : hue; ", "c2 <= red;"), kOk);
}

TEST(Aliases_Types, TheAliasIsTheSameType)
{
    EXPECT_EQ(arch("alias hue is color; signal c2 : hue; ", "process begin c2 <= col; wait; end process;"), kOk);
}

TEST(Aliases_Types, ATypeAliasTakesNoSubtypeOrSignature)
{
    EXPECT_TRUE(mentions(arch("alias hue : color is color; "), "cannot have a subtype indication"));
    EXPECT_TRUE(mentions(arch("alias hue is color [return color]; "), "cannot have a signature"));
}

// ===========================================================================
// 3. ALIASES OF SUBPROGRAMS
// ===========================================================================

namespace
{
    const std::string kFunctions =
        "function twice(x : integer) return integer is begin return x * 2; end; "
        "function twice(x : std_logic) return std_logic is begin return x; end; "
        "procedure hit is begin null; end; "
        "function \"+\"(l, r : color) return color is begin return l; end; ";
}

TEST(Aliases_Subprograms, AnAliasOfASingleSubprogram)
{
    EXPECT_EQ(arch(kFunctions + "alias strike is hit; ", "process begin strike; wait; end process;"), kOk);
}

TEST(Aliases_Subprograms, AnOverloadedNameNeedsASignature)
{
    const std::string message = arch(kFunctions + "alias dbl is twice; ");
    EXPECT_TRUE(mentions(message, "'twice' is overloaded; the alias needs a signature")) << message;
    EXPECT_EQ(arch(kFunctions + "alias dbl is twice [integer return integer]; ", "process begin n <= dbl(2); wait; end process;"), kOk);
    EXPECT_EQ(arch(kFunctions + "alias dbl is twice [std_logic return std_logic]; ", "process begin a <= dbl('1'); wait; end process;"), kOk);
}

TEST(Aliases_Subprograms, OnlyTheSelectedOverloadIsAliased)
{
    const std::string message = arch(kFunctions + "alias dbl is twice [integer return integer]; ", "process begin a <= dbl('1'); wait; end process;");
    EXPECT_TRUE(mentions(message, "Type mismatch for parameter 'x' of 'twice'")) << message;
}

TEST(Aliases_Subprograms, TheSignatureMustMatchAnOverload)
{
    const std::string message = arch(kFunctions + "alias dbl is twice [color return color]; ");
    EXPECT_TRUE(mentions(message, "No subprogram 'twice' has this signature")) << message;
    EXPECT_TRUE(mentions(arch(kFunctions + "alias dbl is twice [integer, integer return integer]; "), "has this signature"));
    EXPECT_TRUE(mentions(arch(kFunctions + "alias dbl is hit [integer]; "), "has this signature"));
    EXPECT_EQ(arch(kFunctions + "alias strike is hit [ ]; ", "process begin strike; wait; end process;"), kOk);
}

TEST(Aliases_Subprograms, AnAliasOfAnOperator)
{
    EXPECT_EQ(arch(kFunctions + "alias \"plus\" is \"+\" [color, color return color]; ", ""), "\"plus\" is not an operator that can be overloaded");
    EXPECT_EQ(arch(kFunctions + "alias \"-\" is \"+\" [color, color return color]; ", "process begin col <= red - blue; wait; end process;"), kOk)
        << "an alias can give a function the name of another operator";
}

TEST(Aliases_Subprograms, TheAliasedNameMustBeASubprogram)
{
    EXPECT_TRUE(mentions(arch("alias x is n [integer return integer]; "), "An alias with a signature must name a subprogram"));
    EXPECT_TRUE(mentions(arch(kFunctions + "alias x : integer is twice; "), "cannot have a subtype indication"));
}

TEST(Aliases_Subprograms, AliasesJoinTheOverloadsOfTheirName)
{
    EXPECT_EQ(arch(kFunctions + "function dbl(x : color) return color is begin return x; end; alias dbl is twice [integer return integer]; ",
                   "process begin n <= dbl(3); col <= dbl(red); wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch(kFunctions + "function dbl(x : integer) return integer is begin return x; end; alias dbl is twice [integer return integer]; "),
                         "already declared under the name 'dbl'"));
}

// ===========================================================================
// 4. DECLARING AND SPECIFYING ATTRIBUTES
// ===========================================================================

TEST(Attributes_Declarations, AnAttributeHasATypeAndAName)
{
    EXPECT_EQ(arch("attribute keep : boolean; attribute width : integer; subtype nib is std_logic_vector(3 downto 0); attribute loc : nib; "), kOk);
    EXPECT_TRUE(mentions(arch("attribute loc : std_logic_vector(3 downto 0); "), "Expected ';', but found '('")) << "an attribute has a type mark, not a constrained type";
    EXPECT_TRUE(mentions(arch("attribute keep : nothing; "), "Unknown type 'nothing'"));
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep : integer; "), "'keep' is already declared"));
    EXPECT_TRUE(mentions(arch("attribute a : boolean; "), "'a' is already declared"));
}

TEST(Attributes_Specifications, EverySignalConstantAndVariableClass)
{
    EXPECT_EQ(arch("attribute keep : boolean; attribute keep of a, b : signal is true; "), kOk);
    EXPECT_EQ(arch("attribute w : integer; constant k : integer := 3; attribute w of k : constant is 8; "), kOk);
    EXPECT_EQ(arch("attribute w : integer; ", "process variable v : integer; attribute w of v : variable is 1; begin wait; end process;"), kOk);
}

TEST(Attributes_Specifications, TypesSubtypesComponentsAndSubprograms)
{
    EXPECT_EQ(arch("attribute tag : integer; attribute tag of color : type is 1; subtype small is integer range 0 to 3; attribute tag of small : subtype is 2; "), kOk);
    EXPECT_EQ(arch("attribute tag : integer; function f return integer is begin return 1; end; attribute tag of f : function is 3; "), kOk);
    EXPECT_EQ(arch("attribute tag : integer; procedure hit is begin null; end; attribute tag of hit : procedure is 3; "), kOk);
}

TEST(Attributes_Specifications, LiteralsAndUnits)
{
    EXPECT_EQ(arch("attribute code : integer; attribute code of red, green : literal is 7; "), kOk);
    EXPECT_EQ(arch("attribute code : integer; type dist is range 0 to 100 units um; mm = 1000 um; end units; attribute code of mm : units is 3; "), kOk);
    EXPECT_TRUE(mentions(arch("attribute code : integer; attribute code of yellow : literal is 7; "), "is not an enumeration literal of a type declared in this region"));
    EXPECT_TRUE(mentions(arch("attribute code : integer; attribute code of fs : units is 7; "), "is not a physical unit of a type declared in this region"))
        << "fs belongs to time, which the prelude declares";
}

TEST(Attributes_Specifications, LabelsAreSpecifiedAfterTheirStatements)
{
    EXPECT_EQ(arch("attribute tag : integer; attribute tag of main : label is 1; ", "main : process begin wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch("attribute tag : integer; attribute tag of missing : label is 1; ", "main : process begin wait; end process;"),
                         "'missing' is not a label of a statement in this region"));
    EXPECT_TRUE(mentions(arch("attribute tag : integer; attribute tag of main : label is 1; attribute tag of main : label is 2; ", "main : process begin wait; end process;"),
                         "already specified for 'main'"));
}

TEST(Attributes_Specifications, LabelsInsideAProcess)
{
    EXPECT_EQ(arch("", "process attribute tag of inner : label is 1; attribute tag : integer; begin inner : for i in 0 to 1 loop null; end loop; wait; end process;"),
              "'tag' is not an attribute; declare it with 'attribute tag : <type>;' first");
    EXPECT_EQ(arch("attribute tag : integer; ", "process attribute tag of inner : label is 1; begin inner : for i in 0 to 1 loop null; end loop; wait; end process;"), kOk);
}

TEST(Attributes_Specifications, OthersAndAllCoverTheRemainingItems)
{
    EXPECT_EQ(arch("attribute keep : boolean; attribute keep of a : signal is true; attribute keep of others : signal is false; "), kOk);
    EXPECT_EQ(arch("attribute keep : boolean; attribute keep of all : signal is true; "), kOk);
    EXPECT_EQ(arch("attribute code : integer; attribute code of all : literal is 1; "), kOk);
    EXPECT_EQ(arch("attribute tag : integer; attribute tag of all : label is 1; ", "l1 : process begin wait; end process; l2 : process begin wait; end process;"), kOk);
}

TEST(Attributes_Specifications, ADuplicateSpecificationIsRejected)
{
    const std::string message = arch("attribute keep : boolean; attribute keep of a : signal is true; attribute keep of a : signal is false; ");
    EXPECT_TRUE(mentions(message, "The attribute 'keep' is already specified for 'a'")) << message;
}

TEST(Attributes_Specifications, TheItemMustBeOfTheClassNamed)
{
    const std::string message = arch("attribute keep : boolean; attribute keep of n : constant is true; ");
    EXPECT_TRUE(mentions(message, "'n' is a signal, which is not what the specification names (constant)")) << message;
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of color : signal is true; "), "'color' is a type"));
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of a : type is true; "), "'a' is a signal"));
}

TEST(Attributes_Specifications, TheItemMustBeDeclaredInTheSameRegion)
{
    const std::string message = arch("attribute keep : boolean; ", "process attribute keep of a : signal is true; begin wait; end process;");
    EXPECT_TRUE(mentions(message, "declared in an enclosing region")) << message;
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of nothing : signal is true; "), "'nothing' is not declared"));
}

TEST(Attributes_Specifications, PortsBelongToTheEntity)
{
    const std::string message = analysisError("entity t is port (i : in std_logic); end t; architecture r of t is attribute keep : boolean; "
                                              "attribute keep of i : signal is true; begin end r;");
    EXPECT_TRUE(mentions(message, "'i' is a port, which is not what the specification names (signal)")) << message;
}

TEST(Attributes_Specifications, TheAttributeMustBeDeclaredFirst)
{
    EXPECT_TRUE(mentions(arch("attribute keep of a : signal is true; "), "'keep' is not an attribute"));
    EXPECT_TRUE(mentions(arch("attribute keep of a : signal is true; attribute keep : boolean; "), "'keep' is not an attribute"));
    EXPECT_TRUE(mentions(arch("attribute a of b : signal is true; "), "'a' is not an attribute"));
}

TEST(Attributes_Specifications, ValuesAreTypeChecked)
{
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of a : signal is 1; "), "Type mismatch"));
    EXPECT_TRUE(mentions(arch("subtype small is integer range 0 to 3; attribute w : small; attribute w of a : signal is 9; "), "outside the range"));
    EXPECT_TRUE(mentions(arch("subtype nib is std_logic_vector(3 downto 0); attribute loc : nib; attribute loc of a : signal is \"01\"; "), "element"));
}

TEST(Attributes_Specifications, ValuesMustBeConstant)
{
    const std::string message = arch("attribute w : integer; attribute w of a : signal is n; ");
    EXPECT_TRUE(mentions(message, "The value of an attribute must be constant")) << message;
    EXPECT_EQ(arch("constant k : integer := 3; attribute w : integer; attribute w of a : signal is k * 2; "), kOk);
}

TEST(Attributes_Specifications, ADesignUnitIsSpecifiedInItsOwnDeclarativePart)
{
    // LRM 7.2: an attribute of an entity or an architecture is specified in the declarative part of that very unit.
    EXPECT_EQ(analysisError("entity t is end t; architecture r of t is attribute a1 : integer; attribute a1 of r : architecture is 2; begin end r;"), kOk);

    const std::string entity = analysisError("entity t is end t; architecture r of t is attribute a1 : integer; attribute a1 of t : entity is 1; begin end r;");
    EXPECT_TRUE(mentions(entity, "An attribute of an entity can only be specified in the declarative part of that entity, so 't' cannot be given one here"))
        << entity;
    EXPECT_TRUE(mentions(arch("attribute a1 : integer; attribute a1 of nothing : entity is 1; "), "declarative part of that entity"));

    const std::string other = arch("attribute a1 : integer; attribute a1 of other : architecture is 1; ");
    EXPECT_TRUE(mentions(other, "An attribute of an architecture can only be specified in the declarative part of that architecture, so 'other' cannot "
                                "be given one here")) << other;
}

TEST(Attributes_Specifications, AnArchitectureIsNotSpecifiedFromAProcessOrASubprogram)
{
    // `arch()` analyzes the architecture `a` of the entity `t`.
    EXPECT_EQ(arch("attribute a1 : integer; attribute a1 of a : architecture is 1; "), kOk);
    EXPECT_TRUE(mentions(proc("attribute a1 : integer; ", "null;", "attribute a1 of a : architecture is 1;"), "declarative part of that architecture"));
    const std::string subprogram = arch("attribute a1 : integer; procedure touch is attribute a1 of a : architecture is 1; begin end;");
    EXPECT_TRUE(mentions(subprogram, "declarative part of that architecture")) << subprogram;
}

TEST(Attributes_Specifications, AllOrOthersOfTheEntityClassNameNothingInAnArchitecture)
{
    // `all` and `others` name the items of the class declared in this declarative part, and no entity is declared in one.
    EXPECT_EQ(arch("attribute a1 : integer; attribute a2 : integer; attribute a1 of all : entity is 1; attribute a2 of others : entity is 2; "), kOk);
}

TEST(Attributes_Specifications, AnArchitectureAttributeIsSpecifiedOnce)
{
    EXPECT_TRUE(mentions(arch("attribute a1 : integer; attribute a1 of a : architecture is 1; attribute a1 of a : architecture is 2; "),
                         "The attribute 'a1' is already specified for 'a'"));
    EXPECT_EQ(arch("attribute a1 : integer; attribute a2 : integer; attribute a1 of a : architecture is 1; attribute a2 of a : architecture is 2; "), kOk);
}

TEST(Attributes_Specifications, ClassesWithNothingToName)
{
    for (const char* entityClass : { "configuration", "package", "group", "file", "property", "sequence" })
    {
        const std::string message = arch(std::string("attribute a1 : integer; attribute a1 of x : ") + entityClass + " is 1; ");
        EXPECT_TRUE(mentions(message, "none can be declared in a design")) << entityClass << ": " << message;
    }
}

// ===========================================================================
// 5. READING ATTRIBUTES
// ===========================================================================

TEST(Attributes_Reading, ReadTheValueOfAnItem)
{
    EXPECT_EQ(arch("attribute w : integer; attribute w of a : signal is 8; ", "process begin n <= a'w; wait; end process;"), kOk);
    EXPECT_EQ(arch("attribute keep : boolean; attribute keep of col : signal is true; ", "process begin p <= col'keep; wait; end process;"), kOk);
    EXPECT_EQ(arch("attribute tag : integer; attribute tag of color : type is 1; ", "process begin n <= color'tag; wait; end process;"), kOk);
    EXPECT_EQ(arch("attribute code : integer; attribute code of red : literal is 7; ", "process begin n <= red'code; wait; end process;"), kOk);
    EXPECT_EQ(arch("attribute tag : integer; attribute tag of main : label is 1; ", "main : process begin wait; end process; process begin n <= main'tag; wait; end process;"), kOk);
}

TEST(Attributes_Reading, TheValueHasTheTypeOfTheAttribute)
{
    EXPECT_TRUE(mentions(arch("attribute w : integer; attribute w of a : signal is 8; ", "process begin a <= a'w; wait; end process;"), "Type mismatch"));
    EXPECT_EQ(arch("subtype small is integer range 0 to 15; attribute w : small; attribute w of a : signal is 8; ", "process begin n <= a'w; wait; end process;"), kOk);
}

TEST(Attributes_Reading, AValueIsFoldedIntoConstants)
{
    EXPECT_EQ(arch("attribute w : integer; attribute w of a : signal is 4; constant k : integer := a'w; signal s : std_logic_vector(k - 1 downto 0); "), kOk);
    const std::string message = arch("attribute w : integer; attribute w of a : signal is 4; constant k : integer := a'w; signal s : std_logic_vector(k - 1 downto 0); ",
                                     "process begin s <= \"000\"; wait; end process;");
    EXPECT_TRUE(mentions(message, "element")) << "k is 4, so s has 4 elements: " << message;
}

TEST(Attributes_Reading, AnItemWithoutTheAttribute)
{
    const std::string message = arch("attribute w : integer; ", "process begin n <= a'w; wait; end process;");
    EXPECT_TRUE(mentions(message, "The attribute 'w' has no value for 'a'; specify it with 'attribute w of a")) << message;
}

TEST(Attributes_Reading, UnknownAttributes)
{
    EXPECT_TRUE(mentions(arch("", "process begin n <= a'nothing; wait; end process;"), "Unknown attribute 'nothing'"));
    EXPECT_TRUE(mentions(arch("signal w : integer; ", "process begin n <= a'w; wait; end process;"), "Unknown attribute 'w'"))
        << "only an attribute declaration makes a name an attribute";
}

TEST(Attributes_Reading, OnlyPlainNamesHaveUserAttributes)
{
    const std::string message = arch("attribute w : integer; attribute w of v8 : signal is 1; ", "process begin n <= v8(1)'w; wait; end process;");
    EXPECT_TRUE(mentions(message, "can only be read from a plain name")) << message;
    EXPECT_TRUE(mentions(arch("attribute w : integer; ", "process begin n <= nothing'w; wait; end process;"), "'nothing' is not declared"));
}

TEST(Attributes_Reading, SignaturesSelectASubprogram)
{
    const std::string f = "function f(x : integer) return integer is begin return x; end; function f(x : color) return integer is begin return 1; end; "
                          "attribute tag : integer; attribute tag of f : function is 5; ";
    EXPECT_EQ(arch(f, "process begin n <= f[integer return integer]'tag; wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch(f, "process begin n <= f[boolean return integer]'tag; wait; end process;"), "has this signature"));
    EXPECT_TRUE(mentions(arch("attribute tag : integer; attribute tag of a : signal is 1; ", "process begin n <= a[integer return integer]'tag; wait; end process;"),
                         "A signature can only follow the name of a subprogram"));
}

TEST(Attributes_Reading, PredefinedAttributesTakeNoSignature)
{
    EXPECT_TRUE(mentions(arch("", "process begin n <= v8[integer return integer]'length; wait; end process;"), "does not take a signature"));
}

TEST(Attributes_Reading, AttributesAreNotValues)
{
    EXPECT_TRUE(mentions(arch("attribute w : integer; ", "process begin n <= w; wait; end process;"), "is an attribute, not a value"));
}
