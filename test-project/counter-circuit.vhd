ENTITY counterCircuit IS
    PORT (
        x : OUT STD_LOGIC;
    );
END ENTITY counterCircuit;

ARCHITECTURE behavioral OF counterCircuit IS

    COMPONENT clock
        PORT (
            clk_out : OUT STD_LOGIC
        );
    END COMPONENT;

    COMPONENT counter
        PORT (
            clk : IN STD_LOGIC;
            reset : IN STD_LOGIC;
            count : OUT STD_LOGIC_VECTOR(31 DOWNTO 0)
        );
    END COMPONENT;

    SIGNAL clk_signal : STD_LOGIC;
    SIGNAL reset_signal : STD_LOGIC;
    SIGNAL count_signal : STD_LOGIC_VECTOR(31 DOWNTO 0);

BEGIN

    clk_inst : clock
    PORT MAP(
        clk_out => clk_signal
    );

    counter_inst : counter
    PORT MAP(
        clk => clk_signal,
        reset => reset_signal,
        count => count_signal
    );

    PROCESS
    BEGIN
        reset_signal <= '1';
        WAIT FOR 1 fs;
        reset_signal <= '0';
        WAIT;
    END PROCESS;

END ARCHITECTURE behavioral;