LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;

ENTITY counter IS
    GENERIC (
        WIDTH : INTEGER := 32
    );
    PORT (
        clk : IN STD_LOGIC;
        reset : IN STD_LOGIC;
        count : OUT UNSIGNED(WIDTH - 1 DOWNTO 0)
    );
END ENTITY counter;

ARCHITECTURE behavioral OF counter IS

    SIGNAL count_internal : UNSIGNED(WIDTH - 1 DOWNTO 0);

BEGIN

    PROCESS (clk, reset)
    BEGIN
        IF reset = '1' THEN
            count_internal <= x"00000000";
        ELSIF clk'event AND clk = '1' THEN
            count_internal <= count_internal + 1;
        END IF;
    END PROCESS;

    count <= count_internal;

END ARCHITECTURE behavioral;