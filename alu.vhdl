ENTITY alu IS
    PORT (
        A : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        B : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
        sel : IN STD_LOGIC_VECTOR(1 DOWNTO 0); -- AND/XOR/ADD/LSL
        res : OUT STD_LOGIC_VECTOR(31 DOWNTO 0)
    );
END ENTITY alu;

ARCHITECTURE behavioral OF alu IS
BEGIN

    res <= A AND B WHEN sel = "00" ELSE
            A XOR B WHEN sel = "01" ELSE
            A + B WHEN sel = "10" ELSE
            A SLL B(5 DOWNTO 0);

END ARCHITECTURE behavioral;