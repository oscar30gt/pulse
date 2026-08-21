ENTITY sr_flip_flop IS
    PORT (
        set : IN STD_LOGIC;
        reset : IN STD_LOGIC;
        q : OUT STD_LOGIC;
        qNot : OUT STD_LOGIC
    );
END ENTITY sr_flip_flop;

ARCHITECTURE behavioral OF sr_flip_flop IS
BEGIN

    q <= qNot NOR reset;
    qNot <= q NOR set;

END ARCHITECTURE behavioral;