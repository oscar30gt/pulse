ENTITY clock IS
    PORT (
        clk_out : OUT STD_LOGIC
    );
END ENTITY clock;

ARCHITECTURE behavioral OF clock IS

    SIGNAL clk_out_internal : STD_LOGIC;

BEGIN

    clk_out <= clk_out_internal;

    PROCESS
    BEGIN
        IF clk_out_internal = '0' THEN
            clk_out_internal <= '1';
        ELSE
            clk_out_internal <= '0';
        END IF;
        WAIT FOR 5 fs;
    END PROCESS;

END ARCHITECTURE behavioral;