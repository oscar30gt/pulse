ENTITY counter IS
    PORT (
        clk : IN STD_LOGIC;
        reset : IN STD_LOGIC;
        count : OUT STD_LOGIC_VECTOR(31 DOWNTO 0);
    );
END ENTITY counter;

ARCHITECTURE behavioral OF counter IS

    SIGNAL count_internal : STD_LOGIC_VECTOR(31 DOWNTO 0);

BEGIN

    PROCESS (clk, reset)
    BEGIN
        IF reset = '1' THEN
            count_internal <= x"00000000";
        ELSIF clk = '1' THEN
            count_internal <= count_internal + x"00000001";
        END IF;
    END PROCESS;

    count <= count_internal;

END ARCHITECTURE behavioral;