
-- Renesas M16C Design Contest 2005
-- Project M1747
-- Portable Website
--
-- Expansion Bus Interface Logic
--

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

entity glue is
    Port ( clk : in std_logic;
    	   clko : out std_logic;
           rd : in std_logic;
           wr : in std_logic;
           bhe : in std_logic;
           cs0 : in std_logic;
           a10 : in std_logic;
           a11 : in std_logic;
		   a0 : in std_logic;
           wt : in std_logic;
           ce0 : out std_logic;
           ce1 : out std_logic;
           reg : out std_logic;
           iord : out std_logic;
           iowr : out std_logic;
           we : out std_logic;
           rdy : out std_logic;
           oe : out std_logic);
end glue;


architecture Behavioral of glue is
		
signal clki : std_logic;

begin

--
-- PCMCIA attribute address pin is an extra address line
--
reg <= a10;

--
-- generate chip selects for 8/16 bit operation
--
chip_selects: process(cs0, bhe, a0)
begin
	ce0 <= '1';
	ce1 <= '1';
	if cs0='0' and a0='0' then 
		ce0 <= '0';
	end if;
	if cs0='0' and bhe='0' then
		ce1 <= '0';
	end if;
end process chip_selects;

--
-- generate read and write strobes a11 determines if the address space is common or i/o
--
read_strobes: process(rd, a11)
begin
	oe <= '1';
	iord <= '1';
	if rd='0' and a11='0' then
		oe <= '0';
	end if;
	if rd='0' and a11='1' then
		iord <= '0';
	end if;
end process read_strobes;

write_strobes: process(wr, a11)
begin
	we <= '1';
	iowr <= '1';
	if wr='0' and a11='0' then
		we <= '0';
	end if;
	if wr='0' and a11='1' then
		iowr <= '0';
	end if;
end process write_strobes;

-- pass the ready signal straight thru
rdy <= wt;
 
--
-- prevent keepers from being placed on important, but currently unused bus lines (clk,bhe)
--
dummy_circuit: process(clk,clki)
begin
	if clk'event and clk='1' then
		clki <= not clki;
	end if;
	clko <= clki;
end process dummy_circuit;

end Behavioral;
