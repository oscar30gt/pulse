ENTITY top IS
END ENTITY top;

ARCHITECTURE behavioral OF top IS

    COMPONENT clock
        PORT (
            clk_out : OUT STD_LOGIC
        );
    END COMPONENT;

    COMPONENT counter
        PORT (
            clk : IN STD_LOGIC;
            reset : IN STD_LOGIC;
            count : OUT UNSIGNED(31 DOWNTO 0)
        );
    END COMPONENT;

    SIGNAL reset : STD_LOGIC := '1';

    SIGNAL clk: STD_LOGIC;
    SIGNAL count : UNSIGNED(31 DOWNTO 0);

BEGIN

    clk_inst : clock
    PORT MAP(
        clk_out => clk
    );

    counter_inst : counter
    PORT MAP(
        clk => clk,
        reset => reset,
        count => count
    );

    PROCESS
    BEGIN
        WAIT FOR 1 fs;
        reset <= '0';
        WAIT;
    END PROCESS;

END ARCHITECTURE behavioral;