-- ============================================================================
-- Complex VHDL Testbench Source File for AST Parser Validation
-- This file intentionally includes complex structures, mixed types, and 
-- unconventional expressions to test the syntactic robustness of the AST builder.
-- ============================================================================

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity complex_test_entity is
    port (
        clk         : in  std_logic;
        rst_n       : in  randomType;
        data_in     : in  std_logic_vector(7 downto 0);
        data_out    : out std_logic_vector(40 to -50 * 16 + 5, 4);
        flag_inout  : inout  boolean
    );
end entity complex_test_entity;

architecture structural_behavioral of complex_test_entity is

    -- Component declaration for instantiation testing
    component sub_unit is
        port (
            sub_clk : in std_logic;
            sub_val : out std_logic_vector(3 downto 0)
        );
    end component sub_unit;

    -- Signal declarations with various type specs, arrays, and initial values
    signal internal_sig_a : std_logic_vector(7 downto 0) := x"A5";
    signal internal_sig_b : integer := 42;
    signal internal_flag  : boolean := true;
    signal weird_mix      : std_logic_vector(3 downto 0) := "0101";

begin
    a <= true;
    b <= 'X';
    c <= 16UX"F";

    -- Concurrent Signal Assignment with a complex When-Else expression
    -- Combines function calls, arithmetic, logic operations, and un-type-safe operands
    data_out <= (unsigned(data_in) + 10) when (flag_inout and (internal_sig_b > 10)) else
                to_integer(unsigned(internal_sig_a)) * (-2) when not internal_flag else
                x"FFFF";

    -- With-Select statement (WithClause testing)
    -- Uses attribute expressions and arithmetic inside the selector
    with (data_in'left + internal_sig_b) select
        internal_sig_a <= x"00" when 0 to 3,
                          x"FF" when 4,
                          x"55" when others;

    -- Component instantiation with port mapping
    u_sub_inst : sub_unit
        port map (
            sub_clk => clk,
            sub_val => weird_mix
        );

    -- Process statement with label, sensitivity list, nested controls, and wait states
    process_label : process(clk, rst_n, internal_sig_a)
    begin
        if rst_n = '0' then
            internal_flag <= false;
            wait for 10 ns;
        elsif rising_edge(clk) then
            -- Complex nested conditional statements combining attributes, 
            -- invalid logical/arithmetic mixing (for parser stress testing), and shifts
            if (internal_sig_a'event and (internal_flag + 5)) then
                internal_sig_b <= to_integer(unsigned(data_in)) sll 2;
            elsif (to_integer(unsigned(data_in)) and 0) then
                wait for 100 us;
            else
                wait;
            end if;
        else
            wait for 500 ps;
        end if;
    end process process_label;

end architecture structural_behavioral;