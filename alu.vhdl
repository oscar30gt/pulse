ENTITY alu IS
    PORT (
        A : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        B : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        sel : IN STD_LOGIC_VECTOR(1 DOWNTO 0); -- AND/XOR/ADD/LSL
        res : OUT STD_LOGIC_VECTOR(31 DOWNTO 0)
    );
END ENTITY alu;

ARCHITECTURE behavioral OF alu IS

    COMPONENT mux_4x32 IS
        PORT (
            in0 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
            in1 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
            in2 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
            in3 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
            sel : IN STD_LOGIC_VECTOR(1 DOWNTO 0);
            _out : OUT STD_LOGIC_VECTOR(31 DOWNTO 0)
        );
    END COMPONENT mux_4x32;
    SIGNAL and_res, xor_res, add_res, lsl_res : STD_LOGIC_VECTOR(31 DOWNTO 0);

BEGIN

    and_res <= A AND B;
    xor_res <= A XOR B;
    add_res <= A + B;
    lsl_res <= A SLL B(5 DOWNTO 0);

    -- MUX
    MUX_INST : mux_4x32 PORT MAP(
        in0 => and_res,
        in1 => xor_res,
        in2 => add_res,
        in3 => lsl_res,
        sel => sel,
        _out => res
    );

END ARCHITECTURE behavioral;