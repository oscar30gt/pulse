ENTITY mux_4x32 IS
    PORT (
        in0 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        in1 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        in2 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        in3 : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        sel : IN STD_LOGIC_VECTOR(1 DOWNTO 0);
        _out : OUT STD_LOGIC_VECTOR(31 DOWNTO 0)
    );
END ENTITY mux_4x32;

ARCHITECTURE behavioral OF mux_4x32 IS
BEGIN

    _out <= in0 WHEN sel = "00" ELSE
            in1 WHEN sel = "01" ELSE
            in2 WHEN sel = "10" ELSE
            in3;

END ARCHITECTURE behavioral;