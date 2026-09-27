// expressions.test.cc — typing of expressions: operators (LRM and numeric_std width
// rules), literals resolved from context, aggregates, indexing, slicing, conversions and
// attributes. Rejected programs assert a fragment of their message.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    /// Signals shared by most tests.
    const std::string kSignals =
        "signal x, y, z : std_logic; signal p, q : boolean; signal n, m : integer; signal r : real; signal t : time; "
        "signal v4, w4 : std_logic_vector(3 downto 0); signal v8 : std_logic_vector(7 downto 0); "
        "signal u4 : unsigned(3 downto 0); signal u8 : unsigned(7 downto 0); signal u12 : unsigned(11 downto 0); "
        "signal s4 : signed(3 downto 0); signal s8 : signed(7 downto 0); ";

    /// The statements run in one process, so they count as a single driver of every signal they assign.
    std::string check(const std::string& body, const std::string& extraDeclarations = "")
    {
        return analysisError(inArchitecture(kSignals + extraDeclarations, "process begin " + body + " wait; end process;"));
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

// ---- Integer and real operators -------------------------------------------------------------------

TEST(Semantic_Operators, IntegerArithmetic)
{
    EXPECT_EQ(check("n <= m + 1 * 2 mod 3 - m rem 2; n <= -m; n <= abs m; n <= m ** 2; n <= m / 2;"), kOk);
}

TEST(Semantic_Operators, RealArithmetic)
{
    EXPECT_EQ(check("r <= r * 2.0 + 1.5; r <= r / 2.0; r <= -r; r <= abs r; r <= r ** 2;"), kOk);
}

TEST(Semantic_Operators, IntegersAndRealsDoNotMix)
{
    EXPECT_TRUE(mentions(check("r <= r * 2;"), "cannot be applied"));
    EXPECT_TRUE(mentions(check("n <= m + 1.5;"), "cannot be applied"));
    EXPECT_TRUE(mentions(check("r <= n;"), "Type mismatch"));
}

TEST(Semantic_Operators, ModAndRemNeedIntegers)
{
    EXPECT_TRUE(mentions(check("r <= r mod 2.0;"), "needs two integers"));
}

TEST(Semantic_Operators, ExponentMustBeAnInteger)
{
    EXPECT_TRUE(mentions(check("n <= m ** 2.0;"), "exponent"));
}

TEST(Semantic_Operators, ResultOfAnOperationHasTheOperandType)
{
    EXPECT_TRUE(mentions(check("x <= n + 1;"), "Type mismatch"));
}

TEST(Semantic_Operators, ConstantExpressionsFoldForRangeChecks)
{
    EXPECT_TRUE(mentions(check("n <= 2147483647 + 1;", ""), "outside the range"));
}

// ---- Relational operators -------------------------------------------------------------------------

TEST(Semantic_Relational, ComparisonsProduceBooleans)
{
    EXPECT_EQ(check("p <= n = m; p <= n /= 3; p <= r < 1.5; p <= x = '1'; p <= t >= 5 ns; p <= v4 = w4; p <= u4 < 3;"), kOk);
}

TEST(Semantic_Relational, ComparingDifferentTypesIsRejected)
{
    EXPECT_TRUE(mentions(check("p <= n = r;"), "same type"));
    EXPECT_TRUE(mentions(check("p <= x = n;"), "same type"));
    EXPECT_TRUE(mentions(check("p <= v4 = u4;"), "cannot be applied"));
}

TEST(Semantic_Relational, ResultIsBooleanNotStdLogic)
{
    EXPECT_TRUE(mentions(check("x <= n = m;"), "Type mismatch"));
}

TEST(Semantic_Relational, RecordsCanOnlyBeComparedForEquality)
{
    const std::string rec = "type pt is record a : integer; end record; signal e, f : pt;";
    EXPECT_EQ(check("p <= e = f;", rec), kOk);
    EXPECT_TRUE(mentions(check("p <= e < f;", rec), "only scalar"));
}

TEST(Semantic_Relational, VectorLiteralsTakeTheirTypeFromTheOtherOperand)
{
    EXPECT_EQ(check("p <= v4 = \"1010\"; p <= \"1010\" = v4; p <= v4 = x\"A\";"), kOk);
    EXPECT_NE(check("p <= v4 = \"1012\";"), kOk);
}

// ---- Logical operators ----------------------------------------------------------------------------

TEST(Semantic_Logical, ScalarsAndVectors)
{
    EXPECT_EQ(check("z <= x and y; z <= x nand y; z <= not x; p <= p or q; p <= not p; v4 <= v4 xor w4; v4 <= not v4;"), kOk);
}

TEST(Semantic_Logical, OperandsMustHaveTheSameTypeAndLength)
{
    EXPECT_TRUE(mentions(check("z <= x and p;"), "same type"));
    EXPECT_EQ(check("v4 <= v4 and v8(3 downto 0) and w4;"), kOk);
    EXPECT_TRUE(mentions(check("v8 <= v8 and v4;"), "same length"));
}

TEST(Semantic_Logical, IntegersHaveNoLogicalOperators)
{
    EXPECT_TRUE(mentions(check("n <= n and m;"), "logical operators work on"));
    EXPECT_TRUE(mentions(check("n <= not m;"), "'not' works on"));
}

TEST(Semantic_Logical, LiteralsFollowTheOtherOperand)
{
    EXPECT_EQ(check("z <= x and '1'; v4 <= v4 and \"1100\"; v4 <= v4 or (others => '0');"), kOk);
    EXPECT_TRUE(mentions(check("v4 <= v4 and \"110\";"), "3 element"));
}

// ---- Shifts ---------------------------------------------------------------------------------------

TEST(Semantic_Shifts, VectorsShiftByIntegers)
{
    EXPECT_EQ(check("v8 <= v8 sll 2; u8 <= u8 srl n; s8 <= s8 sra 1; u8 <= u8 rol 3; v4 <= \"0011\" sll 1;"), kOk);
}

TEST(Semantic_Shifts, ScalarsAndRealShiftAmountsAreRejected)
{
    EXPECT_TRUE(mentions(check("x <= x sll 1;"), "vectors"));
    EXPECT_TRUE(mentions(check("v8 <= v8 sll 1.5;"), "shift amount"));
}

TEST(Semantic_Shifts, ArithmeticShiftsAndRotationsToTheRight)
{
    // numeric_std defines every shift for unsigned and signed; std_logic_1164 the logical shifts and the rotations.
    EXPECT_EQ(check("u8 <= u8 sla 1; s8 <= s8 sla 2; u8 <= u8 ror n; v8 <= v8 ror 2;"), kOk);

    const std::string message = check("x <= x ror 1;");
    EXPECT_TRUE(mentions(message, "Operator 'ror'")) << message;
    EXPECT_TRUE(mentions(message, "shifts and rotations work on vectors")) << message;
    EXPECT_TRUE(mentions(check("u8 <= u8 sla 1.5;"), "the shift amount must be an integer"));
}

// ---- Concatenation --------------------------------------------------------------------------------

TEST(Semantic_Concatenation, LengthsAdd)
{
    EXPECT_EQ(check("v8 <= v4 & w4; v8 <= v4 & \"0000\"; v8 <= \"1111\" & v4; v8 <= x & \"000000\" & y; v8 <= v4 & '1' & \"000\";"), kOk);
}

TEST(Semantic_Concatenation, ResultLengthIsChecked)
{
    const std::string message = check("v8 <= v4 & w4 & v4;");
    EXPECT_TRUE(mentions(message, "Length mismatch")) << message;
    EXPECT_TRUE(mentions(message, "12")) << message;
}

TEST(Semantic_Concatenation, TwoElementsNeedTheContextToPickTheArray)
{
    EXPECT_EQ(check("v2 <= x & y; v2 <= '1' & '0';", "signal v2 : std_logic_vector(1 downto 0);") , kOk);
}

TEST(Semantic_Concatenation, DifferentArrayTypesDoNotConcatenate)
{
    EXPECT_TRUE(mentions(check("v8 <= v4 & u4;"), "same type"));
}

TEST(Semantic_Concatenation, ElementTypeMustMatch)
{
    EXPECT_TRUE(mentions(check("v8 <= v4 & n;"), "can only be extended"));
}

// ---- numeric_std ----------------------------------------------------------------------------------

TEST(Semantic_NumericStd, AdditionTakesTheWiderOperand)
{
    EXPECT_EQ(check("u8 <= u4 + u8; u8 <= u8 - u4; u8 <= u8 + 1; u8 <= 1 + u8; u4 <= u4 + n; s8 <= s8 + s4;"), kOk);
    EXPECT_TRUE(mentions(check("u4 <= u4 + u8;"), "Length mismatch"));
}

TEST(Semantic_NumericStd, MultiplicationAddsWidths)
{
    EXPECT_EQ(check("u12 <= u4 * u8;"), kOk);
    EXPECT_TRUE(mentions(check("u8 <= u4 * u8;"), "12"));
}

TEST(Semantic_NumericStd, UnsignedAndSignedDoNotMix)
{
    EXPECT_TRUE(mentions(check("u8 <= u8 + s8;"), "cannot be mixed"));
    EXPECT_TRUE(mentions(check("p <= u8 = s8;"), "can only be compared"));
}

TEST(Semantic_NumericStd, StdLogicVectorHasNoArithmetic)
{
    const std::string message = check("v8 <= v8 + v8;");
    EXPECT_TRUE(mentions(message, "no arithmetic")) << message;
    EXPECT_TRUE(mentions(message, "unsigned(a)")) << message;
}

TEST(Semantic_NumericStd, SignedHasNegationAndAbsButUnsignedDoesNot)
{
    EXPECT_EQ(check("s8 <= -s8; s8 <= abs s8;"), kOk);
    EXPECT_TRUE(mentions(check("u8 <= -u8;"), "cannot be applied"));
}

TEST(Semantic_NumericStd, ComparisonsWithIntegers)
{
    EXPECT_EQ(check("p <= u8 > 5; p <= s8 <= -3; p <= u8 = u8; p <= 3 < u8;"), kOk);
}

// ---- Literals and aggregates ----------------------------------------------------------------------

TEST(Semantic_Literals, StringLiteralsTakeTheirTypeFromTheTarget)
{
    EXPECT_EQ(check("v4 <= \"1010\"; u4 <= \"1010\"; s4 <= \"1010\"; v8 <= x\"A5\"; v8 <= \"1010101Z\"; v4 <= \"UXZW\"; v4 <= \"LH-1\";"), kOk);
}

TEST(Semantic_Literals, LengthAndCharacterSetAreChecked)
{
    const std::string message = check("v8 <= \"1010\";");
    EXPECT_TRUE(mentions(message, "4 element")) << message;
    EXPECT_TRUE(mentions(message, "has 8")) << message;
    EXPECT_NE(check("v4 <= \"10a1\";"), kOk);
}

TEST(Semantic_Literals, StringLiteralNeedsAnArrayContext)
{
    EXPECT_TRUE(mentions(check("n <= \"101\";"), "cannot be used where"));
    EXPECT_TRUE(mentions(check("p <= \"101\" = \"101\";"), "cannot be determined"));
}

TEST(Semantic_Literals, CharacterLiteralsMustBeValuesOfTheTarget)
{
    EXPECT_EQ(check("x <= '1'; x <= 'Z'; x <= 'H';"), kOk);
    EXPECT_TRUE(mentions(check("x <= '2';"), "not a value of any visible type"));
    EXPECT_TRUE(mentions(check("x <= 'h';"), "not a value of any visible type")) << "character literals are case sensitive";
    EXPECT_TRUE(mentions(check("n <= '1';"), "Type mismatch"));
}

TEST(Semantic_Aggregates, OthersFillsTheRest)
{
    EXPECT_EQ(check("v8 <= (others => '0'); v8 <= (7 => '1', others => '0'); v8 <= (7 downto 4 => '1', 3 downto 0 => '0'); "
                    "v4 <= ('1', '0', '1', '0'); v4 <= (0 => '1', 1 to 3 => '0');"), kOk);
}

TEST(Semantic_Aggregates, EveryIndexMustBeDefinedExactlyOnce)
{
    EXPECT_TRUE(mentions(check("v4 <= (3 => '1', 2 => '0');"), "defines 2 of the 4"));
    EXPECT_TRUE(mentions(check("v4 <= (3 => '1', 3 => '0', others => '1');"), "defined twice"));
    EXPECT_TRUE(mentions(check("v4 <= ('1', '0', '1');"), "defines 3 of the 4"));
    EXPECT_TRUE(mentions(check("v4 <= ('1', '0', '1', '0', '1');"), "positional elements"));
    EXPECT_TRUE(mentions(check("v4 <= (4 => '1', others => '0');"), "outside the range"));
}

TEST(Semantic_Aggregates, ElementTypesAreChecked)
{
    EXPECT_TRUE(mentions(check("v4 <= (others => n);"), "Type mismatch"));
}

TEST(Semantic_Aggregates, NeedAnExpectedType)
{
    EXPECT_TRUE(mentions(check("p <= (x, y) = (x, y);"), "cannot be determined"));
    EXPECT_TRUE(mentions(check("n <= (1, 2);"), "cannot be used where"));
}

TEST(Semantic_Aggregates, ArraysOfArrays)
{
    const std::string mem = "subtype byte is std_logic_vector(7 downto 0); type ram is array (0 to 3) of byte; signal ram1 : ram;";
    EXPECT_EQ(check("ram1 <= (others => (others => '0')); ram1 <= (x\"00\", x\"11\", x\"22\", x\"33\"); v8 <= ram1(1);", mem), kOk);
    EXPECT_EQ(check("x <= ram1(1)(3);", mem), kOk);
    EXPECT_TRUE(mentions(check("v8 <= ram1(4);", mem), "out of bounds"));
}

// ---- Indexing and slicing -------------------------------------------------------------------------

TEST(Semantic_Indexing, IndicesAreCheckedAgainstTheBounds)
{
    EXPECT_EQ(check("x <= v8(0); x <= v8(7); x <= v8(n); x <= v8(n + 1);"), kOk);

    const std::string message = check("x <= v8(8);");
    EXPECT_TRUE(mentions(message, "Index 8 is out of bounds")) << message;
    EXPECT_TRUE(mentions(message, "valid indices are 0 to 7")) << message;
    EXPECT_TRUE(mentions(check("x <= v8(-1);"), "outside the range"));
}

TEST(Semantic_Indexing, IndexMustBeAnInteger)
{
    EXPECT_TRUE(mentions(check("x <= v8(x);"), "Type mismatch"));
    EXPECT_TRUE(mentions(check("x <= v8(1.5);"), "Type mismatch"));
}

TEST(Semantic_Indexing, OnlyArraysCanBeIndexed)
{
    EXPECT_TRUE(mentions(check("x <= n(1);"), "cannot be indexed"));
    EXPECT_TRUE(mentions(check("x <= x(0);"), "cannot be indexed"));
}

TEST(Semantic_Indexing, WrongNumberOfIndices)
{
    EXPECT_TRUE(mentions(check("x <= v8(1, 2);"), "index value"));
}

TEST(Semantic_Indexing, IndexedTargetsAreAssignable)
{
    EXPECT_EQ(check("v8(3) <= '1'; v8(3 downto 0) <= v4; u8(7 downto 4) <= u4;"), kOk);
    EXPECT_TRUE(mentions(check("v8(3) <= v4;"), "Type mismatch"));
}

TEST(Semantic_Slicing, DirectionAndBoundsAreChecked)
{
    EXPECT_EQ(check("v4 <= v8(3 downto 0); v4 <= v8(7 downto 4); v4 <= v8(n + 3 downto n);"), kOk);
    EXPECT_TRUE(mentions(check("v4 <= v8(0 to 3);"), "direction"));
    EXPECT_TRUE(mentions(check("v4 <= v8(9 downto 6);"), "out of bounds"));
}

TEST(Semantic_Slicing, SliceLengthMustMatchTheTarget)
{
    const std::string message = check("v4 <= v8(4 downto 0);");
    EXPECT_TRUE(mentions(message, "Length mismatch")) << message;
    EXPECT_TRUE(mentions(message, "5")) << message;
}

TEST(Semantic_Slicing, SlicesKeepTheirArrayType)
{
    EXPECT_TRUE(mentions(check("u4 <= v8(3 downto 0);"), "Type mismatch"));
    EXPECT_EQ(check("u4 <= u8(3 downto 0) + 1;"), kOk);
}

TEST(Semantic_Slicing, AscendingArrays)
{
    const std::string decl = "type asc is array (0 to 7) of std_logic; signal a : asc; signal b : asc;";
    EXPECT_EQ(check("x <= a(3); a(0 to 3) <= b(4 to 7);", decl), kOk);
    EXPECT_TRUE(mentions(check("a(3 downto 0) <= b(3 downto 0);", decl), "direction"));
}

// ---- Conversions ----------------------------------------------------------------------------------

TEST(Semantic_Conversions, CloselyRelatedTypes)
{
    EXPECT_EQ(check("u4 <= unsigned(v4); v4 <= std_logic_vector(u4); s4 <= signed(u4); n <= integer(r); r <= real(n); u8 <= unsigned(v8);"), kOk);
}

TEST(Semantic_Conversions, LengthIsCheckedWhenStatic)
{
    const std::string message = check("u4 <= unsigned(v8);");
    EXPECT_TRUE(mentions(message, "Length mismatch")) << message;

    // A constrained conversion target is checked against the operand itself.
    EXPECT_TRUE(mentions(check("u4 <= nibble(v8);", "subtype nibble is unsigned(3 downto 0);"), "lengths differ"));
}

TEST(Semantic_Conversions, UnrelatedTypesAreRejected)
{
    EXPECT_TRUE(mentions(check("x <= std_logic(v4);"), "not closely related"));
    EXPECT_TRUE(mentions(check("n <= integer(x);"), "not closely related"));
}

TEST(Semantic_Conversions, LiteralsNeedATypeOfTheirOwn)
{
    EXPECT_TRUE(mentions(check("u4 <= unsigned(\"1010\");"), "ambiguous"));
}

TEST(Semantic_Conversions, UnknownFunctionsAreReported)
{
    const std::string message = check("n <= conv_integer(u4);");
    EXPECT_TRUE(mentions(message, "'conv_integer' is not declared")) << message;
}

// ---- Attributes -----------------------------------------------------------------------------------

TEST(Semantic_Attributes, ArrayAttributes)
{
    EXPECT_EQ(check("n <= v8'length; n <= v8'high; n <= v8'low; n <= v8'left; n <= v8'right;"), kOk);
    EXPECT_TRUE(mentions(check("p <= v8'ascending;"), "Unknown attribute 'ascending'"));
}

TEST(Semantic_Attributes, ScalarTypeAttributes)
{
    EXPECT_EQ(check("n <= integer'high; n <= integer'low; n <= natural'high; x <= std_logic'left;"), kOk);
    EXPECT_TRUE(mentions(check("n <= n'high;"), "needs an array or a scalar type"));
}

TEST(Semantic_Attributes, EventWorksOnAnySignalButNothingElse)
{
    EXPECT_EQ(check("p <= x'event; p <= n'event; p <= v8'event; p <= v8(3)'event; p <= t'event;"), kOk);
    EXPECT_TRUE(mentions(check("p <= c'event;", "constant c : integer := 1;"), "signal or a port"));
    EXPECT_TRUE(mentions(check("x <= x'event;"), "Type mismatch"));
}

TEST(Semantic_Attributes, RangeAttributeOnlyWhereARangeIsExpected)
{
    EXPECT_TRUE(mentions(check("n <= v8'range;"), "where a range is expected"));
    EXPECT_EQ(check("v4 <= v8(v4'range);"), kOk);
}

TEST(Semantic_Attributes, OnlyTheSixValueAttributesAndRangesArePredefined)
{
    const std::string color = "type color is (red, green, blue); signal c : color;";
    for (const char* attribute : { "pos(green)", "val(1)", "succ(red)", "pred(blue)", "image(red)", "ascending", "stable", "base", "path_name" })
    {
        const std::string message = check(std::string("n <= color'") + attribute + ";", color);
        EXPECT_TRUE(mentions(message, "Unknown attribute")) << attribute << ": " << message;
    }
}

TEST(Semantic_Attributes, UnknownAttributeIsReported)
{
    EXPECT_TRUE(mentions(check("n <= v8'banana;"), "Unknown attribute 'banana'"));
}

// ---- Conditions -----------------------------------------------------------------------------------

TEST(Semantic_Conditions, WhenElseConditionsAreBooleanOrStdLogic)
{
    EXPECT_EQ(check("z <= x when p else y; z <= x when y else '0'; z <= x when n = 1 else y when p else '0';"), kOk);
}

TEST(Semantic_Conditions, ConditionOfAnotherTypeIsRejected)
{
    const std::string message = check("z <= x when n else y;");
    EXPECT_TRUE(mentions(message, "must be boolean")) << message;
}

TEST(Semantic_Conditions, EveryBranchIsTypeChecked)
{
    EXPECT_TRUE(mentions(check("z <= x when p else n;"), "Type mismatch"));
    EXPECT_TRUE(mentions(check("z <= n when p else x;"), "Type mismatch"));
}

TEST(Semantic_Conditions, WithSelectIsAConcurrentStatement)
{
    EXPECT_EQ(analysisError(inArchitecture(kSignals, "with n select z <= x when 0, y when others;")), kOk);
}

// ---- Regressions found in review ------------------------------------------------------------------

TEST(Semantic_Review, AggregateOperandsTakeTheIndexRangeOfTheirPartner)
{
    EXPECT_EQ(check("p <= v8 = (others => '0'); p <= (others => '0') = v8; if v8 /= (others => '1') then null; end if;"), kOk);
}

TEST(Semantic_Review, DivisionByAConstantZeroIsAnError)
{
    EXPECT_TRUE(mentions(check("n <= m / 0;"), "Division by zero"));
    EXPECT_TRUE(mentions(check("n <= m mod 0;"), "Division by zero"));
    EXPECT_TRUE(mentions(check("n <= m rem (1 - 1);"), "Division by zero"));
    EXPECT_EQ(check("n <= m / 2;"), kOk);
    EXPECT_EQ(check("r <= r / 0.5;"), kOk);
}

TEST(Semantic_Review, RealRangeConstraints)
{
    const std::string decl = "signal rq : real range 0.0 to 1.0;";
    EXPECT_EQ(check("rq <= 0.5;", decl), kOk);
    EXPECT_TRUE(mentions(check("rq <= 1.5;", decl), "outside the range"));
    EXPECT_TRUE(mentions(check("null;", "signal rq : real range 0 to 1;"), "real numbers"));
    EXPECT_TRUE(mentions(check("null;", "signal rq : real range 1.0 to 0.0;"), "empty"));
}

TEST(Semantic_Review, RangeAttributeNeedsAnArrayOrATypeName)
{
    EXPECT_TRUE(mentions(analysisError(inArchitecture(kSignals, "process begin for i in n'range loop null; end loop; wait; end process;")), "needs an array or a discrete type"));
    EXPECT_EQ(analysisError(inArchitecture(kSignals, "process begin for i in natural'range loop exit; end loop; wait; end process;")), kOk);
}
