/*------------------------------------------------------------------------
 . lan91c113.c
 . This is a driver for SMSC's 91C111 single-chip Ethernet device.
 .
 . Copyright (C) 2001 Standard Microsystems Corporation (SMSC)
 .       Developed by Simple Network Magic Corporation (SNMC)
 . Copyright (C) 1996 by Erik Stahlman (ES)
 .
 . This program is free software; you can redistribute it and/or modify
 . it under the terms of the GNU General Public License as published by
 . the Free Software Foundation; either version 2 of the License, or
 . (at your option) any later version.
 .
 . This program is distributed in the hope that it will be useful,
 . but WITHOUT ANY WARRANTY; without even the implied warranty of
 . MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 . GNU General Public License for more details.
 .
 . You should have received a copy of the GNU General Public License
 . along with this program; if not, write to the Free Software
 . Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 .
 . Information contained in this file was obtained from the LAN91C111
 . manual from SMC.  To get a copy, if you really want one, you can find 
 . information under www.smsc.com.
 . 
 .
 . "Features" of the SMC chip:
 .   Integrated PHY/MAC for 10/100BaseT Operation
 .   Supports internal and external MII
 .   Integrated 8K packet memory
 .   EEPROM interface for configuration
 .
 . Arguments:
 .      io      = for the base address
 .      irq     = for the IRQ
 .      nowait  = 0 for normal wait states, 1 eliminates additional wait states
 .
 . author:
 .      Erik Stahlman                           ( erik@vt.edu )
 .      Daris A Nevil                           ( dnevil@snmc.com )
 .      Pramod B Bhardwaj                       (pramod.bhardwaj@smsc.com)
 .
 .
 . Hardware multicast code from Peter Cammaert ( pc@denkart.be )
 .
 . Sources:
 .    o   SMSC LAN91C111 databook (www.smsc.com)
 .    o   smc9194.c by Erik Stahlman
 .    o   skeleton.c by Donald Becker ( becker@cesdis.gsfc.nasa.gov )
 .
 . History:
 .    07/18/02  bedguy, added support for IME9400 board
 .    06/03/02  Greg Ungerer, added support for M5249C3 board
 .    09/24/01  Pramod B Bhardwaj, Added the changes for Kernel 2.4
 .    08/21/01  Pramod B Bhardwaj Added support for RevB of LAN91C111
 .    04/25/01  Daris A Nevil  Initial public release through SMSC
 .    03/16/01  Daris A Nevil  Modified smc9194.c for use with LAN91C111
 ----------------------------------------------------------------------------*/

#include <common.h>
#include <command.h>
#include <config.h>
#include "lan91c113.h"
#include <net.h>

// Use power-down feature of the chip
#define POWER_DOWN      1
#define CONFIG_SMC16BITONLY     1

static const char version[] =
        "SMSC LAN91C113 Driver U-BOOT Support for Odd Byte zhoukejun@gmail.com\n";

/*------------------------------------------------------------------------
 .
 . Configuration options, for the experienced user to change.
 .
 -------------------------------------------------------------------------*/

/*
 . Do you want to use 32 bit xfers?  This should work on all chips, as
 . the chipset is designed to accommodate them.
*/
#undef USE_32_BIT

/*
 .the LAN91C111 can be at any of the following port addresses.  To change,
 .for a slightly different card, you can add it to the array.  Keep in
 .mind that the array must end in zero.
*/


static unsigned int smc_portlist[] = { 0x0C000000+0x300, 0 };


/*
 . Wait time for memory to be free.  This probably shouldn't be
 . tuned that much, as waiting for this means nothing else happens
 . in the system
*/
#define MEMORY_WAIT_TIME 16

/*
 . DEBUGGING LEVELS
 .
 . 0 for normal operation
 . 1 for slightly more details
 . >2 for various levels of increasingly useless information
 .    2 for interrupt tracking, status flags
 .    3 for packet info
 .    4 for complete packet dumps
*/


#if (SMC_DEBUG > 2 )
#define PRINTK3(args...) printf(args)
#else
#define PRINTK3(args...)
#endif

#if SMC_DEBUG > 1
#define PRINTK2(args...) printf(args)
#else
#define PRINTK2(args...)
#endif

#ifdef SMC_DEBUG
#define PRINTK(args...) printf(args)
#else
#define PRINTK(args...)
#endif


/*------------------------------------------------------------------------
 .
 . The internal workings of the driver.  If you are changing anything
 . here with the SMC stuff, you should have the datasheet and know
 . what you are doing.
 .
 -------------------------------------------------------------------------*/
#define CARDNAME "LAN91C113"

// Memory sizing constant
#define LAN91C111_MEMORY_MULTIPLIER     (1024*2)

#ifndef CONFIG_SMC91111_BASE
#define CONFIG_SMC91111_BASE 0x0C000300
#endif

#define SMC_BASE_ADDRESS CONFIG_SMC91111_BASE

#define SMC_PHY_ADDR 0x0000

#define SMC_ALLOC_MAX_TRY 5
#define SMC_TX_TIMEOUT 30

#define ETH_ZLEN 60

/*-----------------------------------------------------------------
 .
 .  The driver can be entered at any of the following entry points.
 .
 .------------------------------------------------------------------  */

extern int eth_init(bd_t *bd);
extern void eth_halt(void);
extern int eth_rx(void);
extern int eth_send(volatile void *packet, int length);


/*
 . This is called by  register_netdev().  It is responsible for
 . checking the portlist for the SMC9000 series chipset.  If it finds
 . one, then it will initialize the device, find the hardware information,
 . and sets up the appropriate device parameters.
 . NOTE: Interrupts are *OFF* when this procedure is called.
 .
 . NB:This shouldn't be static since it is referred to externally.
*/
int smc_init(void);

/*
 . This is called by  unregister_netdev().  It is responsible for
 . cleaning up before the driver is finally unregistered and discarded.
*/
void smc_destructor(void);

/*
 . The kernel calls this function when someone wants to use the net_device,
 . typically 'ifconfig ethX up'.
*/
static int smc_open(bd_t *bd);


/*
 . This is called by the kernel in response to 'ifconfig ethX down'.  It
 . is responsible for cleaning up everything that the open routine
 . does, and maybe putting the card into a powerdown state.
*/
static int smc_close(void);

/*
 . Configures the PHY through the MII Management interface
*/
#ifndef CONFIG_SMC91111_EXT_PHY
static void smc_phy_configure(void);
#endif /* !CONFIG_SMC91111_EXT_PHY */


/*---------------------------------------------------------------
 .
 . Interrupt level calls..
 .
 ----------------------------------------------------------------*/

/*
 . This is a separate procedure to handle the receipt of a packet, to
 . leave the interrupt code looking slightly cleaner
*/
inline static int smc_rcv(void);
/*
 . This handles a TX interrupt, which is only called when an error
 . relating to a packet is sent.
*/
inline static void smc_tx(void);

/*
 . This handles interrupts generated from PHY register 18
*/

int smc_get_ethaddr(bd_t *bd);
int get_rom_mac(uchar *v_rom_mac);

/*
 ------------------------------------------------------------
 .
 . Internal routines
 .
 ------------------------------------------------------------
*/

/*
 . Test if a given location contains a chip, trying to cause as
 . little damage as possible if it's not a SMC chip.
*/
static int smc_probe(unsigned int ioaddr);

/*
 . A rather simple routine to print out a packet for debugging purposes.
*/

#if SMC_DEBUG > 2
static void print_packet( byte *, int );
#endif

#define tx_done(dev) 1

/* this is called to actually send the packet to the chip */
static int smc_send_packet(volatile void *packet, int packet_length);

/* this does a soft reset on the device */
static void smc_reset(void);

/* Enable Interrupts, Receive, and Transmit */
static void smc_enable(void);

/* this puts the device in an inactive state */
static void smc_shutdown(void);


/* Routines to Read and Write the PHY Registers across the
   MII Management Interface
*/

static word smc_read_phy_register(byte phyreg);
static void smc_write_phy_register(byte phyreg, word phydata);

static char unsigned smc_mac_addr[6] = {0x02, 0x80, 0xad, 0x20, 0x31, 0xb8};

/*
 * This function must be called before smc_open() if you want to override
 * the default mac address.
 */

void smc_set_mac_addr(const unsigned char *addr) {
	int i;

	for (i=0; i < sizeof(smc_mac_addr); i++){
		smc_mac_addr[i] = addr[i];
	}
}

static int poll4int (byte mask, int timeout)
{
	int tmo = get_timer (0) + timeout * CFG_HZ;
	int is_timeout = 0;
	word old_bank = SMC_inw (BSR_REG);

	PRINTK2 ("Polling...\n");
	SMC_SELECT_BANK (2);
	while ((SMC_inw (INT_REG) & mask) == 0) {
		if (get_timer (0) >= tmo) {
			is_timeout = 1;
			break;
		}
	}

	/* restore old bank selection */
	SMC_SELECT_BANK (old_bank);

	if (is_timeout)
		return 1;
	else
		return 0;
}

static inline void smc_wait_mmu_release_complete (void)
{
	int count = 0;
	/* assume bank 2 selected */
	while (SMC_inw (MMU_CMD_REG) & MC_BUSY) {
		udelay (1);	/* Wait until not busy */
		if (++count > 200)
			break;
	}
}


/*
 . Function: smc_reset( struct device* dev )
 . Purpose:
 .      This sets the SMC91111 chip to its normal state, hopefully from whatever
 .      mess that any other DOS driver has put it in.
 .
 . Maybe I should reset more registers to defaults in here?  SOFTRST  should
 . do that for me.
 .
 . Method:
 .      1.  send a SOFT RESET
 .      2.  wait for it to finish
 .      3.  enable autorelease mode
 .      4.  reset the memory management unit
 .      5.  clear all interrupts
 .
*/
static void smc_reset(void)
{
	unsigned short	status_test;

	status_test=SMC_inw(14);
	if((status_test&0xff00)!=0x3300)
	{
		printf("ethid=%04x...\n",status_test);
	}
        PRINTK2("%s:smc_reset\n", CARDNAME);

	

        /* This resets the registers mostly to defaults, but doesn't
           affect EEPROM.  That seems unnecessary */
        SMC_SELECT_BANK( 0 );
        SMC_outw( RCR_SOFTRST,   RCR_REG );

        /* Setup the Configuration Register */
        /* This is necessary because the CONFIG_REG is not affected */
        /* by a soft reset */

        SMC_SELECT_BANK( 1 );
        SMC_outw( CONFIG_DEFAULT,   CONFIG_REG);

        /* Setup for fast accesses if requested */
        /* If the card/system can't handle it then there will */
        /* be no recovery except for a hard reset or power cycle */

#ifdef POWER_DOWN
        /* Release from possible power-down state */
        /* Configuration register is not affected by Soft Reset */
        SMC_SELECT_BANK( 1 );
        SMC_outw( SMC_inw(   CONFIG_REG ) | CONFIG_EPH_POWER_EN,
                  CONFIG_REG  );
#endif

        SMC_SELECT_BANK( 0 );

        /* this should pause enough for the chip to be happy */
        udelay(10000);

        /* Disable transmit and receive functionality */
        SMC_outw( RCR_CLEAR,   RCR_REG );
        SMC_outw( TCR_CLEAR,   TCR_REG );

        /* set the control register to automatically
           release successfully transmitted packets, to make the best
           use out of our limited memory */
        SMC_SELECT_BANK( 1 );
        SMC_outw( SMC_inw(   CTL_REG ) | CTL_AUTO_RELEASE ,   CTL_REG );

        /* Reset the MMU */
        SMC_SELECT_BANK( 2 );
	smc_wait_mmu_release_complete (); //zkj check
        SMC_outw( MC_RESET,   MMU_CMD_REG );

	while (SMC_inw (MMU_CMD_REG) & MC_BUSY)//zkj check
		udelay (1);	/* Wait until not busy */

        /* Note:  It doesn't seem that waiting for the MMU busy is needed here,
           but this is a place where future chipsets _COULD_ break.  Be wary
           of issuing another MMU command right after this */

        /* Disable all interrupts */
#if defined(CONFIG_SMC16BITONLY)
        SMC_outw( 0,   INT_REG );
#else
        SMC_outb( 0,   IM_REG );
#endif
}

/*
 . Function: smc_enable
 . Purpose: let the chip talk to the outside work
 . Method:
 .      1.  Enable the transmitter
 .      2.  Enable the receiver
 .      3.  Enable interrupts
*/
static void smc_enable(void)
{
        PRINTK2("%s:smc_enable\n", CARDNAME);

        SMC_SELECT_BANK( 0 );
        /* see the header file for options in TCR/RCR DEFAULT*/
      	// SMC_outw( lp->tcr_cur_mode,   TCR_REG );
      	//  SMC_outw( lp->rcr_cur_mode,   RCR_REG );
	SMC_outw( 0x0001,   TCR_REG );
        SMC_outw( 0x0300,   RCR_REG );

        /* now, enable interrupts */
        SMC_SELECT_BANK( 2 );
#if defined(CONFIG_SMC16BITONLY)
        SMC_outw( (SMC_INTERRUPT_MASK << 8),   INT_REG );
#else
        SMC_outb( SMC_INTERRUPT_MASK,   IM_REG );
#endif
}

/*
 . Function: smc_shutdown
 . Purpose:  closes down the SMC91xxx chip.
 . Method:
 .      1. zero the interrupt mask
 .      2. clear the enable receive flag
 .      3. clear the enable xmit flags
 .
 . TODO:
 .   (1) maybe utilize power down mode.
 .      Why not yet?  Because while the chip will go into power down mode,
 .      the manual says that it will wake up in response to any I/O requests
 .      in the register space.   Empirical results do not show this working.
*/
static void smc_shutdown(void)
{
        PRINTK2(CARDNAME ":smc_shutdown\n");

        /* no more interrupts for me */
        SMC_SELECT_BANK( 2 );
#if defined(CONFIG_SMC16BITONLY)
        SMC_outw( 0,   INT_REG );
#else
        SMC_outb( 0,   IM_REG );
#endif

        /* and tell the card to stay away from that nasty outside world */
        SMC_SELECT_BANK( 0 );
#if defined(CONFIG_SMC16BITONLY)
        SMC_outw( RCR_CLEAR,   RCR_REG );
        SMC_outw( TCR_CLEAR,   TCR_REG );
#else
        SMC_outb( RCR_CLEAR,   RCR_REG );
        SMC_outb( TCR_CLEAR,   TCR_REG );
#endif

#ifdef POWER_DOWN
        /* finally, shut the chip down */
        SMC_SELECT_BANK( 1 );
        SMC_outw( SMC_inw(   CONFIG_REG ) & ~CONFIG_EPH_POWER_EN,
                  CONFIG_REG  );
#endif
}



static int smc_send_packet(volatile void *packet, int packet_length)
{
	byte packet_no;
	unsigned long ioaddr;
	byte *buf;
        int length;
        unsigned short numPages;
	int try = 0;
        word time_out;
	byte status;
	byte saved_pnr;
	word saved_ptr;

	/* save PTR and PNR registers before manipulation */
	SMC_SELECT_BANK (2);
	saved_pnr = SMC_inb( PN_REG );
	saved_ptr = SMC_inw( PTR_REG );

        PRINTK3("%s:smc_send_packet\n", CARDNAME);

        length = ETH_ZLEN < packet_length ? packet_length : ETH_ZLEN;

                
        /*
        ** The MMU wants the number of pages to be the number of 256 bytes
        ** 'pages', minus 1 ( since a packet can't ever have 0 pages :) )
        **
        ** The 91C111 ignores the size bits, but the code is left intact
        ** for backwards and future compatibility.
        **
        ** Pkt size for allocating is data length +6 (for additional status
        ** words, length and ctl!)
        **
        ** If odd size then last byte is included in this header.
        */
        numPages =   ((length & 0xfffe) + 6);
        numPages >>= 8; // Divide by 256

        if (numPages > 7 ) {
                printf("%s: Far too big packet error. \n", CARDNAME);
                return 0;
        }

        /* now, try to allocate the memory */
        SMC_SELECT_BANK( 2 );
        SMC_outw( MC_ALLOC | numPages,   MMU_CMD_REG );

#if 0 //zkj. Method 1st.
        /*
        . Performance Hack
        .
        . wait a short amount of time.. if I can send a packet now, I send
        . it now.  Otherwise, I enable an interrupt and wait for one to be
        . available.
        .
        . I could have handled this a slightly different way, by checking to
        . see if any memory was available in the FREE MEMORY register.  However,
        . either way, I need to generate an allocation, and the allocation works
        . no matter what, so I saw no point in checking free memory.
        */
        time_out = MEMORY_WAIT_TIME;
        do {
                status = SMC_inb(   INT_REG );
                if ( status & IM_ALLOC_INT ) {
                        /* acknowledge the interrupt */
#if defined(CONFIG_SMC16BITONLY)
                        SMC_outw( IM_ALLOC_INT | (SMC_inb(  IM_REG) << 8),
                                  INT_REG );
#else
                        SMC_outb( IM_ALLOC_INT,   INT_REG );
#endif
                        break;
                }
        } while ( -- time_out );

        if ( !time_out ) {
                /* oh well, wait until the chip finds memory later */
                SMC_ENABLE_INT( IM_ALLOC_INT );

                /* Check the status bit one more time just in case */
                /* it snuk in between the time we last checked it */
                /* and when we set the interrupt bit */
                status = SMC_inb(   INT_REG );
                if ( !(status & IM_ALLOC_INT) ) {
                        PRINTK2("%s: memory allocation deferred. \n",
                                CARDNAME);
                        /* it's deferred, but I'll handle it later */
                        return 0;
                        }

                /* Looks like it did sneak in, so disable */
                /* the interrupt */
                SMC_DISABLE_INT( IM_ALLOC_INT );
        }
#else //zkj Method 2nd.

	/* FIXME: the ALLOC_INT bit never gets set *
	 * so the following will always give a	   *
	 * memory allocation error.		   *
	 * same code works in armboot though	   *
	 * -ro
	 */

again:
	try++;
	time_out = MEMORY_WAIT_TIME;
	do {
		status = SMC_inb (INT_REG);
		if (status & IM_ALLOC_INT) {
			/* acknowledge the interrupt */
			SMC_outb (IM_ALLOC_INT, INT_REG);
			break;
		}
	} while (--time_out);

	if (!time_out) {
		PRINTK2 ("%s: memory allocation, try %d failed ...\n",
			 CARDNAME, try);
		if (try < SMC_ALLOC_MAX_TRY)
			goto again;
		else
			return 0;
	}

	PRINTK2 ("%s: memory allocation, try %d succeeded ...\n",
		 CARDNAME, try);

#endif

        ioaddr = SMC_BASE_ADDRESS;

        buf = (byte *)packet;

        /* If I get here, I _know_ there is a packet slot waiting for me */
        packet_no = SMC_inb(   AR_REG );
        if ( packet_no & AR_FAILED ) {
                /* or isn't there?  BAD CHIP! */
                printf ("%s: Memory allocation failed. \n",
                        CARDNAME);
                return 0;
        }

        /* we have a packet address, so tell the card to use it */
        SMC_outb( packet_no,   PN_REG );

        /* point to the beginning of the packet */
        SMC_outw( PTR_AUTOINC ,   PTR_REG );

        PRINTK3("%s: Trying to xmit packet of length %x\n",
                CARDNAME, length);

#if SMC_DEBUG > 2
        printf("Transmitting Packet\n");
        print_packet( buf, length );
#endif

        /* send the packet length ( +6 for status, length and ctl byte )
           and the status word ( set to zeros ) */
#ifdef USE_32_BIT
#if defined(CONFIG_SMC16BITONLY)
        SMC_outl(  (length +6 ) ,   DATA_REG );
#else
        SMC_outl(  (length +6 ) << 16 ,   DATA_REG );
#endif
#else
        SMC_outw( 0,   DATA_REG );
        /* send the packet length ( +6 for status words, length, and ctl*/
#if defined(CONFIG_SMC16BITONLY)
//        SMC_outw( (length+6) & 0xFFFF,  DATA_REG );
        SMC_outw( (length+6),  DATA_REG );
#else
        SMC_outb( (length+6) & 0xFF,  DATA_REG );
        SMC_outb( (length+6) >> 8 ,   DATA_REG );
#endif
#endif

        /* send the actual data
         . I _think_ it's faster to send the longs first, and then
         . mop up by sending the last word.  It depends heavily
         . on alignment, at least on the 486.  Maybe it would be
         . a good idea to check which is optimal?  But that could take
         . almost as much time as is saved?
        */
#ifdef USE_32_BIT
        SMC_outsl(  DATA_REG, buf,  length >> 2 );
        if ( length & 0x2  )
#if defined(CONFIG_SMC16BITONLY)
                SMC_outw(*((word *)(buf + (length & 0xFFFFFFFC))),DATA_REG);
#else
                SMC_outw(*((word *)(buf + (length & 0xFFFFFFFC))),DATA_REG);
#endif
#else
        SMC_outsw(  DATA_REG , buf, (length ) >> 1);
#endif // USE_32_BIT

        /* Send the last byte, if there is one.   */
        if ( (length & 1) == 0 ) {
                SMC_outw( 0,   DATA_REG );
        } else {
#if defined(CONFIG_SMC16BITONLY)
                SMC_outw( buf[length -1 ] | 0x2000,   DATA_REG );
#else
                SMC_outb( buf[length -1 ],   DATA_REG );
                SMC_outb( 0x20,   DATA_REG); // Set odd bit in CONTROL BYTE
#endif
        }

        /* enable the interrupts */
        SMC_ENABLE_INT( (IM_TX_INT | IM_TX_EMPTY_INT) );//zkj check.

        /* and let the chipset deal with it */
        SMC_outw( MC_ENQUEUE ,   MMU_CMD_REG );

	/* poll for TX INT */
	/* if (poll4int (IM_TX_INT, SMC_TX_TIMEOUT)) { */
	/* poll for TX_EMPTY INT - autorelease enabled */
	if (poll4int(IM_TX_EMPTY_INT, SMC_TX_TIMEOUT)) {
		/* sending failed */
		PRINTK2 ("%s: TX timeout, sending failed...\n", CARDNAME);

		/* release packet */
		/* no need to release, MMU does that now */

		/* wait for MMU getting ready (low) */
		while (SMC_inw (MMU_CMD_REG) & MC_BUSY) {
			udelay (10);
		}

		PRINTK2 ("MMU ready\n");


		return 0;
	} else {
		/* ack. int */
		SMC_outb (IM_TX_EMPTY_INT, INT_REG);
		/* SMC_outb (IM_TX_INT, INT_REG); */
		PRINTK2 ("%s: Sent packet of length %d \n", CARDNAME,
			 length);

		/* release packet */
		/* no need to release, MMU does that now */
		/* wait for MMU getting ready (low) */
		while (SMC_inw (MMU_CMD_REG) & MC_BUSY) {
			udelay (10);
		}

		PRINTK2 ("MMU ready\n");


	}

	/* restore previously saved registers */
	SMC_outb( saved_pnr, PN_REG );
	SMC_outw( saved_ptr, PTR_REG );

	return length;
}

/*-------------------------------------------------------------------------
 |
 | smc_init( struct device * dev )
 |   Input parameters:
 |      dev->base_addr == 0, try to find all possible locations
 |      dev->base_addr == 1, return failure code
 |      dev->base_addr == 2, always allocate space,  and return success
 |      dev->base_addr == <anything else>   this is the address to check
 |
 |   Output:
 |      0 --> there is a device
 |      anything else, error
 |
 ---------------------------------------------------------------------------
*/
int smc91c111_init(void)
{
        int i;
        unsigned int base_addr = SMC_BASE_ADDRESS;
	printf("smc91c111_init        \n");
        PRINTK2(CARDNAME ":smc_init\n");

#if	0
        BWSCON = (BWSCON & ~(BWSCON_ST4 | BWSCON_WS4 | BWSCON_DW4)) |
      	//(BWSCON_ST4 | BWSCON_WS4 | BWSCON_DW(4, BWSCON_DW_16));
      	(BWSCON_ST4 | BWSCON_DW(4, BWSCON_DW_16));
    	BANKCON4= BANKCON_Tacs0 | BANKCON_Tcos1 | BANKCON_Tacc4 |
      	BANKCON_Toch1 | BANKCON_Tcah1 | BANKCON_Tacp2 | BANKCON_PMC16; //031201 1data --> 16data/page 

    set_external_irq(IRQ_SMC91C111, EXT_RISING_EDGE, GPIO_PULLUP_DIS);
#endif
        /*  try a specific location */
	printf("1111111111111\n");
        if (base_addr > 0x1ff)
                return smc_probe(base_addr);
        else if ( 0 != base_addr ) 
	{
                printf ("-ENXIO\n");
		return -1;
	}
	printf("2222222222\n");
        /* check every ethernet address */
        for (i = 0; smc_portlist[i]; i++) {
                if ( smc_probe(smc_portlist[i]) ==0)
                        return 0;
        }
                
        /* couldn't find anything */
	printf ("-ENODEV\n");
        return -1;
}


/*-------------------------------------------------------------------------
 |
 | smc_destructor( struct device * dev )
 |   Input parameters:
 |      dev, pointer to the device structure
 |
 |   Output:
 |      None.
 |
 ---------------------------------------------------------------------------
*/
void smc_destructor(void)
{
        PRINTK2(CARDNAME ":smc_destructor\n");
}

/*----------------------------------------------------------------------
 . Function: smc_probe( unsigned int ioaddr )
 .
 . Purpose:
 .      Tests to see if a given ioaddr points to an SMC91111 chip.
 .      Returns a 0 on success
 .
 . Algorithm:
 .      (1) see if the high byte of BANK_SELECT is 0x33
 .      (2) compare the ioaddr with the base register's address
 .      (3) see if I recognize the chip ID in the appropriate register
 .
 .---------------------------------------------------------------------
 */
/*---------------------------------------------------------------
 . Here I do typical initialization tasks.
 .
 . o  Initialize the structure if needed
 . o  print out my vanity message if not done so already
 . o  print out what type of hardware is detected
 . o  print out the ethernet address
 . o  find the IRQ
 . o  set up my private data
 . o  configure the dev structure with my subroutines
 . o  actually GRAB the irq.
 . o  GRAB the region
 .-----------------------------------------------------------------*/
//zkj TODO
static int smc_probe(unsigned int ioaddr )
{
        int i, memory, retval;
        static unsigned version_printed = 0;
        unsigned int    bank;

        const char *version_string;

        /*registers */
        word    revision_register;
        word    base_address_register;
        word    memory_info_register;

        PRINTK2(CARDNAME ":smc_probe\n");
#if	1

        /* First, see if the high byte is 0x33 */
        bank = SMC_inw(   BANK_SELECT );
        if ( (bank & 0xFF00) != 0x3300 ) 
	{
		PRINTK2("SMC_wrong_1\n");
		printf("Can't detect : %04x\n", bank);
		printf ("-ENODEV\n");
		return -1;
	}
        /* The above MIGHT indicate a device, but I need to write to further test this.  */
        SMC_outw( 0x0,   BANK_SELECT );
        bank = SMC_inw(   BANK_SELECT );
        if ( (bank & 0xFF00 ) != 0x3300 )
        {
		printf ("-ENODEV\n");
                retval = -1;
		PRINTK2("SMC_wrong_2\n");
                goto err_out;
        }

        /* well, we've already written once, so hopefully another time won't
           hurt.  This time, I need to switch the bank register to bank 1,
           so I can access the base address register */
        SMC_SELECT_BANK(1);
        base_address_register = SMC_inw(   BASE_REG );
        if ((ioaddr & 0xfff) != (base_address_register >> 3 & 0x3E0))
        {
                printf(CARDNAME ": IOADDR %x doesn't match configuration (%x)."
                        "Probably not a SMC chip\n",
                        ioaddr, base_address_register >> 3 & 0x3E0 );
                /* well, the base address register didn't match.  Must not have
                   been a SMC chip after all. */
		printf ("-ENODEV\n");
                retval = -1;
                goto err_out;
        }
	#endif
        /*  check if the revision register is something that I recognize.
            These might need to be added to later, as future revisions
            could be added.  */
        SMC_SELECT_BANK(3);
        revision_register  = SMC_inw(   REV_REG );
        if ( !chip_ids[ ( revision_register  >> 4 ) & 0xF  ] )
        {
                /* I don't recognize this chip, so... */
                printf(CARDNAME ": IO %x: Unrecognized revision register:"
                        " %x, Contact author. \n",
                        ioaddr, revision_register );
		printf ("-ENODEV\n");
                retval = -1;
                goto err_out;
        }

        /* at this point I'll assume that the chip is an SMC9xxx.
           It might be prudent to check a listing of MAC addresses
           against the hardware address, or do some other tests. */

        if (version_printed++ == 0)
                printf("%s", version);

        /* fill in some of the fields */

        /* Program MAC address if not set... */
        SMC_SELECT_BANK( 1 );
        for (i = 0; (i < 6); i += 2) {
                word address;
                address = SMC_inw(   ADDR0_REG + i ) ;
                if ((address != 0x0000) && (address != 0xffff))
                        break;
        }
        if (i >= 6) {
                /* Set a default MAC address */
                SMC_outw(0xCF00,   ADDR0_REG);
                SMC_outw(0x4952,   ADDR0_REG + 2);
                SMC_outw(0x01C3,   ADDR0_REG + 4);
        }

        /* get the memory information */

        SMC_SELECT_BANK( 0 );
        memory_info_register = SMC_inw(   MIR_REG );
        memory = memory_info_register & (word)0x00ff;
        memory *= LAN91C111_MEMORY_MULTIPLIER;

        /*
         Now, I want to find out more about the chip.  This is sort of
         redundant, but it's cleaner to have it in both, rather than having
         one VERY long probe procedure.
        */
        SMC_SELECT_BANK(3);
        revision_register  = SMC_inw(   REV_REG );
        version_string = chip_ids[ ( revision_register  >> 4 ) & 0xF  ];
        if ( !version_string )
        {
                /* I shouldn't get here because this call was done before.... */
		printf ("-ENODEV\n");
                retval = -1;
		PRINTK2("SMC_wrong_3\n");
                goto err_out;
        }

        /* now, reset the chip, and put it into a known state */
        smc_reset();


        /* Fill in the fields of the device structure with ethernet values. */

        return 0;

err_out:
        return retval;
}

#if SMC_DEBUG > 2
static void print_packet( byte * buf, int length )
{
        int i;
        int remainder;
        int lines;

        printf("Packet of length %d \n", length );

        lines = length / 16;
        remainder = length % 16;

        for ( i = 0; i < lines ; i ++ ) {
                int cur;

                for ( cur = 0; cur < 8; cur ++ ) {
                        byte a, b;

                        a = *(buf ++ );
                        b = *(buf ++ );
                        printf("%02x%02x ", a, b );
                }
                printf("\n");
        }
        for ( i = 0; i < remainder/2 ; i++ ) {
                byte a, b;

                a = *(buf ++ );
                b = *(buf ++ );
                printf("%02x%02x ", a, b );
        }
        printf("\n");
}
#endif

/*
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc ..
 *
 */
static int smc_open(bd_t *bd)
{
        int     i;      /* used to set hw ethernet address */
	int	err;

	if (smc91c111_init() == -1)
	{
		printf ("smc91c111_init() error\n");
		return -1;
	}
        PRINTK2("%s:smc_open\n", CARDNAME);

        /* reset the hardware */

        smc_reset();
        smc_enable();

        /* Configure the PHY */
        smc_phy_configure();

        /*
                According to Becker, I have to set the hardware address
                at this point, because the (l)user can set it with an
                ioctl.  Easily done...
        */
        SMC_SELECT_BANK( 1 );

	err = smc_get_ethaddr (bd);	/* set smc_mac_addr, and sync it with u-boot globals */
	if (err < 0)
	{
		memset (bd->bi_enetaddr, 0, 6);
		return (-1);
	}

        for ( i = 0; i < 6; i += 2 ) {
                word    address;

                address = smc_mac_addr[ i + 1 ] << 8 ;
                address  |= smc_mac_addr[ i ];
                SMC_outw( address,   ADDR0_REG + i );
        }

        return 0;
}



/*-------------------------------------------------------------
 .
 . smc_rcv -  receive a packet from the card
 .
 . There is ( at least ) a packet waiting to be read from
 . chip-memory.
 .
 . o Read the status
 . o If an error, record it
 . o otherwise, read in the packet
 --------------------------------------------------------------
*/
static int smc_rcv(void)
{
        int     packet_number;
        word    status;
        word    packet_length;
	int	is_error = 0;
	byte saved_pnr;
	word saved_ptr;

        /* assume bank 2 */
	SMC_SELECT_BANK(2);
	/* save PTR and PTR registers */
	saved_pnr = SMC_inb( PN_REG );
	saved_ptr = SMC_inw( PTR_REG );


        packet_number = SMC_inw(   RXFIFO_REG );

        if ( packet_number & RXFIFO_REMPTY ) {

                /* we got called , but nothing was on the FIFO */
                PRINTK("%s: WARNING: smc_rcv with nothing on FIFO. \n",
                        CARDNAME);
                /* don't need to restore anything */
                return 0;
        }

        PRINTK3("%s:smc_rcv\n", CARDNAME);

        /*  start reading from the start of the packet */
        SMC_outw( PTR_READ | PTR_RCV | PTR_AUTOINC,   PTR_REG );

        /* First two words are status and packet_length */
        status          = SMC_inw(   DATA_REG );
        packet_length   = SMC_inw(   DATA_REG );

        packet_length &= 0x07ff;  /* mask off top bits */

        PRINTK2("RCV: STATUS %4x LENGTH %4x\n", status, packet_length );

        if ( !(status & RS_ERRORS ) ){
                /* Adjust for having already read the first two words */
                packet_length -= 4;

#ifdef USE_32_BIT
                PRINTK3(" Reading %d dwords (and %d bytes) \n",
                        packet_length >> 2, packet_length & 3 );
                /* QUESTION:  Like in the TX routine, do I want
                   to send the DWORDs or the bytes first, or some
                   mixture.  A mixture might improve already slow PIO
                   performance  */
		SMC_insl( DATA_REG , NetRxPackets[0], packet_length >> 2 );
                /* read the left over bytes */
		if (packet_length & 3) {
			int i;

			byte *tail = (byte *)(NetRxPackets[0] + (packet_length & ~3));
			dword leftover = SMC_inl(DATA_REG);
			for (i=0; i<(packet_length & 3); i++)
				*tail++ = (byte) (leftover >> (8*i)) & 0xff;
		}
#else
                PRINTK3(" Reading %d words and %d byte(s) \n",
                        (packet_length >> 1 ), packet_length & 1 );
		SMC_insw(DATA_REG , NetRxPackets[0], packet_length >> 1);

#endif // USE_32_BIT

#if SMC_DEBUG > 2
		printf("Receiving Packet\n");
		print_packet( NetRxPackets, packet_length );
#endif
        } else {
                /* error ... */
		is_error = 1;
        }

        while ( SMC_inw(   MMU_CMD_REG ) & MC_BUSY )
                udelay(1); // Wait until not busy
        /*  error or good, tell the card to get rid of this packet */
        SMC_outw( MC_RELEASE,   MMU_CMD_REG );

	while ( SMC_inw( MMU_CMD_REG ) & MC_BUSY )
		udelay(1); /* Wait until not busy */

	/* restore saved registers */
	SMC_outb( saved_pnr, PN_REG );

	SMC_outw( saved_ptr, PTR_REG );

	if (!is_error) {
		/* Pass the packet up to the protocol layers. */
		NetReceive(NetRxPackets[0], packet_length);
		return packet_length;
	} else {
		return 0;
	}
}


/*************************************************************************
 . smc_tx
 .
 . Purpose:  Handle a transmit error message.   This will only be called
 .   when an error, because of the AUTO_RELEASE mode.
 .
 . Algorithm:
 .      Save pointer and packet no
 .      Get the packet no from the top of the queue
 .      check if it's valid ( if not, is this an error??? )
 .      read the status word
 .      record the error
 .      ( resend?  Not really, since we don't want old packets around )
 .      Restore saved values
 ************************************************************************/
static void smc_tx(void)
{
        byte saved_packet;
        byte packet_no;
        word tx_status;


        PRINTK3("%s:smc_tx\n", CARDNAME);

        /* assume bank 2  */

        saved_packet = SMC_inb(   PN_REG );
        packet_no = SMC_inw(   RXFIFO_REG );
        packet_no &= 0x7F;

        /* If the TX FIFO is empty then nothing to do */
        if ( packet_no & TXFIFO_TEMPTY )
                return;

        /* select this as the packet to read from */
#if defined(CONFIG_SMC16BITONLY)
        SMC_outw( packet_no,   PN_REG );
#else
        SMC_outb( packet_no,   PN_REG );
#endif

        /* read the first word (status word) from this packet */
        SMC_outw( PTR_AUTOINC | PTR_READ,   PTR_REG );

        tx_status = SMC_inw(   DATA_REG );
        PRINTK3("%s: TX DONE STATUS: %4x \n", CARDNAME, tx_status);

        if ( tx_status & TS_LOSTCAR ) {
		printf("tx_status & TS_LOSTCAR");//zkj
	}
        if ( tx_status & TS_LATCOL  ) {
                printf(
                        "%s: Late collision occurred on last xmit.\n",
                        CARDNAME);
        }
#if 0
        if ( tx_status & TS_16COL ) { ... }
#endif

        if ( tx_status & TS_SUCCESS ) {
                printf("%s: Successful packet caused interrupt \n", CARDNAME);
        }
        /* re-enable transmit */
        SMC_SELECT_BANK( 0 );
        SMC_outw( SMC_inw(   TCR_REG ) | TCR_ENABLE,   TCR_REG );

        /* kill the packet */
        SMC_SELECT_BANK( 2 );
        SMC_outw( MC_FREEPKT,   MMU_CMD_REG );


        /* Don't change Packet Number Reg until busy bit is cleared */
        /* Per LAN91C111 Spec, Page 50 */
        while ( SMC_inw(   MMU_CMD_REG ) & MC_BUSY );

#if defined(CONFIG_SMC16BITONLY)
        SMC_outw( saved_packet,   PN_REG );
#else
        SMC_outb( saved_packet,   PN_REG );
#endif
        return;
}


/*----------------------------------------------------
 . smc_close
 .
 . this makes the board clean up everything that it can
 . and not talk to the outside world.   Caused by
 . an 'ifconfig ethX down'
 .
 -----------------------------------------------------*/
static int smc_close()
{
        PRINTK2("%s:smc_close\n", CARDNAME);

        /* clear everything */
        smc_shutdown();

        return 0;
}



//---PHY CONTROL AND CONFIGURATION-----------------------------------------

#if (SMC_DEBUG > 2 )

/*------------------------------------------------------------
 . Debugging function for viewing MII Management serial bitstream
 .-------------------------------------------------------------*/
static void smc_dump_mii_stream(byte* bits, int size)
{
        int i;

        printf("BIT#:");
        for (i = 0; i < size; ++i)
                {
                printf("%d", i%10);
                }

        printf("\nMDOE:");
        for (i = 0; i < size; ++i)
                {
                if (bits[i] & MII_MDOE)
                        printf("1");
                else
                        printf("0");
                }

        printf("\nMDO :");
        for (i = 0; i < size; ++i)
                {
                if (bits[i] & MII_MDO)
                        printf("1");
                else
                        printf("0");
                }

        printf("\nMDI :");
        for (i = 0; i < size; ++i)
                {
                if (bits[i] & MII_MDI)
                        printf("1");
                else
                        printf("0");
                }

        printf("\n");
}
#endif

/*------------------------------------------------------------
 . Reads a register from the MII Management serial interface
 .-------------------------------------------------------------*/
static word smc_read_phy_register(byte phyreg)
{
        int oldBank;
        int i;
        byte mask;
        word mii_reg;
        byte bits[64];
        int clk_idx = 0;
        int input_idx;
        word phydata;
	byte phyaddr = SMC_PHY_ADDR;

        // 32 consecutive ones on MDO to establish sync
        for (i = 0; i < 32; ++i)
                bits[clk_idx++] = MII_MDOE | MII_MDO;

        // Start code <01>
        bits[clk_idx++] = MII_MDOE;
        bits[clk_idx++] = MII_MDOE | MII_MDO;

        // Read command <10>
        bits[clk_idx++] = MII_MDOE | MII_MDO;
        bits[clk_idx++] = MII_MDOE;

        // Output the PHY address, msb first
        mask = (byte)0x10;
        for (i = 0; i < 5; ++i)
                {
                if (phyaddr & mask)
                        bits[clk_idx++] = MII_MDOE | MII_MDO;
                else
                        bits[clk_idx++] = MII_MDOE;

                // Shift to next lowest bit
                mask >>= 1;
                }

        // Output the phy register number, msb first
        mask = (byte)0x10;
        for (i = 0; i < 5; ++i)
                {
                if (phyreg & mask)
                        bits[clk_idx++] = MII_MDOE | MII_MDO;
                else
                        bits[clk_idx++] = MII_MDOE;

                // Shift to next lowest bit
                mask >>= 1;
                }

        // Tristate and turnaround (2 bit times)
        bits[clk_idx++] = 0;
        //bits[clk_idx++] = 0;

        // Input starts at this bit time
        input_idx = clk_idx;

        // Will input 16 bits
        for (i = 0; i < 16; ++i)
                bits[clk_idx++] = 0;

        // Final clock bit
        bits[clk_idx++] = 0;

        // Save the current bank
        oldBank = SMC_inw(  BANK_SELECT );

        // Select bank 3
        SMC_SELECT_BANK( 3 );

        // Get the current MII register value
        mii_reg = SMC_inw(  MII_REG );

        // Turn off all MII Interface bits
        mii_reg &= ~(MII_MDOE|MII_MCLK|MII_MDI|MII_MDO);

        // Clock all 64 cycles
        for (i = 0; i < sizeof bits; ++i)
                {
                // Clock Low - output data
                SMC_outw( mii_reg | bits[i],  MII_REG );
                udelay(50);


                // Clock Hi - input data
                SMC_outw( mii_reg | bits[i] | MII_MCLK,  MII_REG );
                udelay(50);
                bits[i] |= SMC_inw(  MII_REG ) & MII_MDI;
                }

        // Return to idle state
        // Set clock to low, data to low, and output tristated
        SMC_outw( mii_reg,  MII_REG );
        udelay(50);

        // Restore original bank select
        SMC_SELECT_BANK( oldBank );

        // Recover input data
        phydata = 0;
        for (i = 0; i < 16; ++i)
                {
                phydata <<= 1;

                if (bits[input_idx++] & MII_MDI)
                        phydata |= 0x0001;
                }

#if (SMC_DEBUG > 2 )
        printf("smc_read_phy_register(): phyaddr=%x,phyreg=%x,phydata=%x\n",
                phyaddr, phyreg, phydata);
        smc_dump_mii_stream(bits, sizeof bits);
#endif

        return(phydata);        
}


/*------------------------------------------------------------
 . Writes a register to the MII Management serial interface
 .-------------------------------------------------------------*/
static void smc_write_phy_register(byte phyreg, word phydata)
{
        int oldBank;
        int i;
        word mask;
        word mii_reg;
        byte bits[65];
        int clk_idx = 0;
	byte phyaddr = SMC_PHY_ADDR; 

        // 32 consecutive ones on MDO to establish sync
        for (i = 0; i < 32; ++i)
                bits[clk_idx++] = MII_MDOE | MII_MDO;

        // Start code <01>
        bits[clk_idx++] = MII_MDOE;
        bits[clk_idx++] = MII_MDOE | MII_MDO;

        // Write command <01>
        bits[clk_idx++] = MII_MDOE;
        bits[clk_idx++] = MII_MDOE | MII_MDO;

        // Output the PHY address, msb first
        mask = (byte)0x10;
        for (i = 0; i < 5; ++i)
                {
                if (phyaddr & mask)
                        bits[clk_idx++] = MII_MDOE | MII_MDO;
                else
                        bits[clk_idx++] = MII_MDOE;

                // Shift to next lowest bit
                mask >>= 1;
                }

        // Output the phy register number, msb first
        mask = (byte)0x10;
        for (i = 0; i < 5; ++i)
                {
                if (phyreg & mask)
                        bits[clk_idx++] = MII_MDOE | MII_MDO;
                else
                        bits[clk_idx++] = MII_MDOE;

                // Shift to next lowest bit
                mask >>= 1;
                }

        // Tristate and turnaround (2 bit times)
        bits[clk_idx++] = 0;
        bits[clk_idx++] = 0;

        // Write out 16 bits of data, msb first
        mask = 0x8000;
        for (i = 0; i < 16; ++i)
                {
                if (phydata & mask)
                        bits[clk_idx++] = MII_MDOE | MII_MDO;
                else
                        bits[clk_idx++] = MII_MDOE;

                // Shift to next lowest bit
                mask >>= 1;
                }

        // Final clock bit (tristate)
        bits[clk_idx++] = 0;

        // Save the current bank
        oldBank = SMC_inw(  BANK_SELECT );

        // Select bank 3
        SMC_SELECT_BANK( 3 );

        // Get the current MII register value
        mii_reg = SMC_inw(  MII_REG );

        // Turn off all MII Interface bits
        mii_reg &= ~(MII_MDOE|MII_MCLK|MII_MDI|MII_MDO);

        // Clock all cycles
        for (i = 0; i < sizeof bits; ++i)
                {
                // Clock Low - output data
                SMC_outw( mii_reg | bits[i],  MII_REG );
                udelay(50);


                // Clock Hi - input data
                SMC_outw( mii_reg | bits[i] | MII_MCLK,  MII_REG );
                udelay(50);
                bits[i] |= SMC_inw(  MII_REG ) & MII_MDI;
                }

        // Return to idle state
        // Set clock to low, data to low, and output tristated
        SMC_outw( mii_reg,  MII_REG );
        udelay(50);

        // Restore original bank select
        SMC_SELECT_BANK( oldBank );

#if (SMC_DEBUG > 2 )
        printf("smc_write_phy_register(): phyaddr=%x,phyreg=%x,phydata=%x\n",
                phyaddr, phyreg, phydata);
        smc_dump_mii_stream(bits, sizeof bits);
#endif
}


/*------------------------------------------------------------
 . Finds and reports the PHY address
 .-------------------------------------------------------------*/
static int smc_detect_phy(void)
{
        word phy_id1;
        word phy_id2;
        int phyaddr;
        int found = 0;

        PRINTK3("%s:smc_detect_phy()\n", CARDNAME);

        // Scan all 32 PHY addresses if necessary
        for (phyaddr = 0; phyaddr < 32; ++phyaddr)
                {
                // Read the PHY identifiers
                phy_id1  = smc_read_phy_register(PHY_ID1_REG);
                phy_id2  = smc_read_phy_register(PHY_ID2_REG);

                PRINTK3("%s: phy_id1=%x, phy_id2=%x\n",
                        CARDNAME, phy_id1, phy_id2);

                // Make sure it is a valid identifier   
                if ((phy_id2 > 0x0000) && (phy_id2 < 0xffff) &&
                    (phy_id1 > 0x0000) && (phy_id1 < 0xffff))
                        {
                        if ((phy_id1 != 0x8000) && (phy_id2 != 0x8000))
                                {
                                found = 1;
                                break;
                                }
                        }
                }

        if (!found)
                {
                PRINTK("%s: No PHY found\n", CARDNAME);
                return(0);
                }

        // Set the PHY type
        if ( (phy_id1 == 0x0016) && ((phy_id2 & 0xFFF0) == 0xF840 ) )
                {
                PRINTK("%s: PHY=LAN83C183 (LAN91C111 Internal)\n", CARDNAME);
                }

        if ( (phy_id1 == 0x0282) && ((phy_id2 & 0xFFF0) == 0x1C50) )
                {
                PRINTK("%s: PHY=LAN83C180\n", CARDNAME);
                }

        return(1);
}

/*------------------------------------------------------------
 . Waits the specified number of milliseconds - kernel friendly
 .-------------------------------------------------------------*/
static void smc_wait_ms(unsigned int ms)
{
	udelay(ms*1000);
}



/*------------------------------------------------------------
 . Configures the specified PHY using Autonegotiation. Calls
 . smc_phy_fixed() if the user has requested a certain config.
 .-------------------------------------------------------------*/
static void smc_phy_configure(void)
{
        int timeout;
        byte phyaddr;
        word my_phy_caps; // My PHY capabilities
        word my_ad_caps; // My Advertised capabilities
        word status = 0;
        int failed = 0;

        PRINTK3("%s:smc_program_phy()\n", CARDNAME);

        // Get the detected phy address
        phyaddr = SMC_PHY_ADDR;

        // Reset the PHY, setting all other bits to zero
        smc_write_phy_register(PHY_CNTL_REG, PHY_CNTL_RST);

        // Wait for the reset to complete, or time out
        timeout = 6; // Wait up to 3 seconds
        while (timeout--)
                {
                if (!(smc_read_phy_register(PHY_CNTL_REG)
                    & PHY_CNTL_RST))
                        {
                        // reset complete
                        break;
                        }

                smc_wait_ms(500); // wait 500 millisecs
                }

        if (timeout < 1)
                {
                PRINTK2("%s:PHY reset timed out\n", CARDNAME);
                goto smc_phy_configure_exit;
                }

        // Enable PHY Interrupts (for register 18)
        // Interrupts listed here are disabled
        smc_write_phy_register(PHY_MASK_REG, 
                PHY_INT_LOSSSYNC | PHY_INT_CWRD | PHY_INT_SSD |
                PHY_INT_ESD | PHY_INT_RPOL | PHY_INT_JAB |
                PHY_INT_SPDDET | PHY_INT_DPLXDET);

        /* Configure the Receive/Phy Control register */
        SMC_SELECT_BANK(0);
        SMC_outw(RPC_DEFAULT, RPC_REG);

        // Copy our capabilities from PHY_STAT_REG to PHY_AD_REG
        my_phy_caps = smc_read_phy_register(PHY_STAT_REG);
        my_ad_caps  = PHY_AD_CSMA; // I am CSMA capable

        if (my_phy_caps & PHY_STAT_CAP_T4)
                my_ad_caps |= PHY_AD_T4;

        if (my_phy_caps & PHY_STAT_CAP_TXF)
                my_ad_caps |= PHY_AD_TX_FDX;

        if (my_phy_caps & PHY_STAT_CAP_TXH)
                my_ad_caps |= PHY_AD_TX_HDX;

        if (my_phy_caps & PHY_STAT_CAP_TF)
                my_ad_caps |= PHY_AD_10_FDX;

        if (my_phy_caps & PHY_STAT_CAP_TH)
                my_ad_caps |= PHY_AD_10_HDX;

        // Update our Auto-Neg Advertisement Register
        smc_write_phy_register(PHY_AD_REG, my_ad_caps);

	//zkj. copy from u-boot/driver/net/smc91111.c, I think it is reasonable.
	/* Read the register back.  Without this, it appears that when */
	/* auto-negotiation is restarted, sometimes it isn't ready and */
	/* the link does not come up. */
	smc_read_phy_register(PHY_AD_REG);

        PRINTK2("%s:phy caps=%x\n", CARDNAME, my_phy_caps);
        PRINTK2("%s:phy advertised caps=%x\n", CARDNAME, my_ad_caps);

        // Restart auto-negotiation process in order to advertise my caps
        smc_write_phy_register(PHY_CNTL_REG,
                PHY_CNTL_ANEG_EN | PHY_CNTL_ANEG_RST );

        // Wait for the auto-negotiation to complete.  This may take from
        // 2 to 3 seconds.
        // Wait for the reset to complete, or time out
        timeout = 20; // Wait up to 10 seconds
        while (timeout--)
                {
                status = smc_read_phy_register(PHY_STAT_REG);
                if (status & PHY_STAT_ANEG_ACK)
                        {
                        // auto-negotiate complete
                        break;
                        }

                smc_wait_ms(500); // wait 500 millisecs
                // Restart auto-negotiation if remote fault
                if (status & PHY_STAT_REM_FLT)
                        {
                        PRINTK2("%s:PHY remote fault detected\n", CARDNAME);

                        // Restart auto-negotiation
                        PRINTK2("%s:PHY restarting auto-negotiation\n",
                                CARDNAME);
                        smc_write_phy_register( PHY_CNTL_REG,
                                PHY_CNTL_ANEG_EN | PHY_CNTL_ANEG_RST |
                                PHY_CNTL_SPEED | PHY_CNTL_DPLX);
                        }
                }

        if (timeout < 1)
                {
                printf("%s:PHY auto-negotiate timed out\n",
                        CARDNAME);
                PRINTK2("%s:PHY auto-negotiate timed out\n", CARDNAME);
                failed = 1;
                }

        // Fail if we detected an auto-negotiate remote fault
        if (status & PHY_STAT_REM_FLT)
                {
                printf("%s:PHY remote fault detected\n", CARDNAME);
                PRINTK2("%s:PHY remote fault detected\n", CARDNAME);
                failed = 1;
                }

        // Re-Configure the Receive/Phy Control register
        SMC_outw(RPC_DEFAULT, RPC_REG );

smc_phy_configure_exit:
	return;
}



int eth_init (bd_t *bd)
{
	return (smc_open(bd));
}

void eth_halt (void)
{
	smc_close ();
}

int eth_rx (void)
{
	return smc_rcv ();
}

int eth_send (volatile void *packet, int length)
{
	return smc_send_packet (packet, length);
}

int smc_get_ethaddr (bd_t * bd)
{
	int env_size, rom_valid, env_present = 0, reg;
	char *s = NULL, *e, es[] = "11:22:33:44:55:66";
	char s_env_mac[64];
	uchar v_env_mac[6], v_rom_mac[6], *v_mac;

	env_size = getenv_r ("ethaddr", s_env_mac, sizeof (s_env_mac));
	if ((env_size > 0) && (env_size < sizeof (es))) {	/* exit if env is bad */
		printf ("\n*** ERROR: ethaddr is not set properly!!\n");
		return (-1);
	}

	if (env_size > 0) {
		env_present = 1;
		s = s_env_mac;
	}

	for (reg = 0; reg < 6; ++reg) { /* turn string into mac value */
		v_env_mac[reg] = s ? simple_strtoul (s, &e, 16) : 0;
		if (s)
			s = (*e) ? e + 1 : e;
	}

	rom_valid = get_rom_mac (v_rom_mac);	/* get ROM mac value if any */

	if (!env_present) {	/* if NO env */
		if (rom_valid) {	/* but ROM is valid */
			v_mac = v_rom_mac;
			sprintf (s_env_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
				 v_mac[0], v_mac[1], v_mac[2], v_mac[3],
				 v_mac[4], v_mac[5]);
			setenv ("ethaddr", s_env_mac);
		} else {	/* no env, bad ROM */
			printf ("\n*** ERROR: ethaddr is NOT set !!\n");
			return (-1);
		}
	} else {		/* good env, don't care ROM */
		v_mac = v_env_mac;	/* always use a good env over a ROM */
	}

	if (env_present && rom_valid) { /* if both env and ROM are good */
		if (memcmp (v_env_mac, v_rom_mac, 6) != 0) {
			printf ("\nWarning: MAC addresses don't match:\n");
			printf ("\tHW MAC address:  "
				"%02X:%02X:%02X:%02X:%02X:%02X\n",
				v_rom_mac[0], v_rom_mac[1],
				v_rom_mac[2], v_rom_mac[3],
				v_rom_mac[4], v_rom_mac[5] );
			printf ("\t\"ethaddr\" value: "
				"%02X:%02X:%02X:%02X:%02X:%02X\n",
				v_env_mac[0], v_env_mac[1],
				v_env_mac[2], v_env_mac[3],
				v_env_mac[4], v_env_mac[5]) ;
			debug ("### Set MAC addr from environment\n");
		}
	}
	memcpy (bd->bi_enetaddr, v_mac, 6);	/* update global address to match env (allows env changing) */
	smc_set_mac_addr ((uchar *)v_mac);	/* use old function to update smc default */
	PRINTK("Using MAC Address %02X:%02X:%02X:%02X:%02X:%02X\n", v_mac[0], v_mac[1],
		v_mac[2], v_mac[3], v_mac[4], v_mac[5]);
	return (0);
}

int get_rom_mac (uchar *v_rom_mac)
{
#ifdef HARDCODE_MAC	/* used for testing or to supress run time warnings */
	char hw_mac_addr[] = { 0x02, 0x80, 0xad, 0x20, 0x31, 0xb8 };

	memcpy (v_rom_mac, hw_mac_addr, 6);
	return (1);
#else
	int i;
	int valid_mac = 0;

	SMC_SELECT_BANK (1);
	for (i=0; i<6; i++)
	{
		v_rom_mac[i] = SMC_inb ((ADDR0_REG + i));
		valid_mac |= v_rom_mac[i];
	}

	return (valid_mac ? 1 : 0);
#endif
}
