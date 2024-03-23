/*******************************************************************************

  Intel 82599 Virtual Function driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/types.h>
#include <linux/module.h>

#include "ixgbevf.h"

/* This is the only thing that needs to be changed to adjust the
 * maximum number of ports that the driver can manage.
 */

#define IXGBE_MAX_NIC 8

#define OPTION_UNSET    -1
#define OPTION_DISABLED 0
#define OPTION_ENABLED  1

/* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */

#define IXGBE_PARAM_INIT { [0 ... IXGBE_MAX_NIC] = OPTION_UNSET }
#ifndef module_param_array
/* Module Parameters are always initialized to -1, so that the driver
 * can tell the difference between no user specified value or the
 * user asking for the default value.
 * The true default values are loaded in when ixgbevf_check_options is called.
 *
 * This is a GCC extension to ANSI C.
 * See the item "Labeled Elements in Initializers" in the section
 * "Extensions to the C Language Family" of the GCC documentation.
 */

#define IXGBE_PARAM(X, desc) \
	static const int __devinitdata X[IXGBE_MAX_NIC+1] = IXGBE_PARAM_INIT; \
	MODULE_PARM(X, "1-" __MODULE_STRING(IXGBE_MAX_NIC) "i"); \
	MODULE_PARM_DESC(X, desc);
#else
#define IXGBE_PARAM(X, desc) \
	static int __devinitdata X[IXGBE_MAX_NIC+1] = IXGBE_PARAM_INIT; \
	static unsigned int num_##X; \
	module_param_array_named(X, X, int, &num_##X, 0); \
	MODULE_PARM_DESC(X, desc);
#endif

/* Interrupt Throttle Rate (interrupts/sec)
 *
 * Valid Range: 956-488281 (0=off, 1=dynamic)
 *
 * Default Value: 8000
 */
#define DEFAULT_ITR                 8000
IXGBE_PARAM(InterruptThrottleRate, "Maximum interrupts per second, per vector, (956-488281), default 8000");
#define MAX_ITR       IXGBE_MAX_INT_RATE
#define MIN_ITR       IXGBE_MIN_INT_RATE

/* Rx buffer mode
 *
 * Valid Range: 0-2 0 = 1buf_mode_always, 1 = ps_mode_always and 2 = optimal
 *
 * Default Value: 0
 */
IXGBE_PARAM(RxBufferMode, "0 (default) =use 1 descriptor per packet,\n"
                          "\t\t\t1=use packet split, multiple descriptors per jumbo frame\n"
                          "\t\t\t2 =use 1buf mode for 1500 mtu, packet split for jumbo");

#define IXGBE_RXBUFMODE_1BUF_ALWAYS			0
#define IXGBE_RXBUFMODE_PS_ALWAYS			1
#define IXGBE_RXBUFMODE_OPTIMAL				2
#define IXGBE_DEFAULT_RXBUFMODE	  IXGBE_RXBUFMODE_1BUF_ALWAYS

struct ixgbevf_option {
	enum { enable_option, range_option, list_option } type;
	const char *name;
	const char *err;
	int def;
	union {
		struct { /* range_option info */
			int min;
			int max;
		} r;
		struct { /* list_option info */
			int nr;
			const struct ixgbevf_opt_list {
				int i;
				char *str;
			} *p;
		} l;
	} arg;
};

static int __devinit ixgbevf_validate_option(unsigned int *value,
					     struct ixgbevf_option *opt)
{
	if (*value == OPTION_UNSET) {
		*value = opt->def;
		return 0;
	}

	switch (opt->type) {
	case enable_option:
		switch (*value) {
		case OPTION_ENABLED:
			printk(KERN_INFO "ixgbevf: %s Enabled\n", opt->name);
			return 0;
		case OPTION_DISABLED:
			printk(KERN_INFO "ixgbevf: %s Disabled\n", opt->name);
			return 0;
		}
		break;
	case range_option:
		if (*value >= opt->arg.r.min && *value <= opt->arg.r.max) {
			printk(KERN_INFO "ixgbe: %s set to %d\n", opt->name, *value);
			return 0;
		}
		break;
	case list_option: {
		int i;
		const struct ixgbevf_opt_list *ent;

		for (i = 0; i < opt->arg.l.nr; i++) {
			ent = &opt->arg.l.p[i];
			if (*value == ent->i) {
				if (ent->str[0] != '\0')
					printk(KERN_INFO "%s\n", ent->str);
				return 0;
			}
		}
	}
		break;
	default:
		BUG();
	}

	printk(KERN_INFO "ixgbevf: Invalid %s specified (%d),  %s\n",
	       opt->name, *value, opt->err);
	*value = opt->def;
	return -1;
}

#define LIST_LEN(l) (sizeof(l) / sizeof(l[0]))

/**
 * ixgbevf_check_options - Range Checking for Command Line Parameters
 * @adapter: board private structure
 *
 * This routine checks all command line parameters for valid user
 * input.  If an invalid value is given, or if no user specified
 * value exists, a default value is used.  The final value is stored
 * in a variable in the adapter structure.
 **/
void __devinit ixgbevf_check_options(struct ixgbevf_adapter *adapter)
{
	int bd = adapter->bd_number;

	if (bd >= IXGBE_MAX_NIC) {
		printk(KERN_NOTICE
		       "Warning: no configuration for board #%d\n", bd);
		printk(KERN_NOTICE "Using defaults for all values\n");
#ifndef module_param_array
		bd = IXGBE_MAX_NIC;
#endif
	}

	{ /* Interrupt Throttling Rate */
		static struct ixgbevf_option opt = {
			.type = range_option,
			.name = "Interrupt Throttling Rate (ints/sec)",
			.err  = "using default of "__MODULE_STRING(DEFAULT_ITR),
			.def  = DEFAULT_ITR,
			.arg  = { .r = { .min = MIN_ITR,
					 .max = MAX_ITR }}
		};

#ifdef module_param_array
		if (num_InterruptThrottleRate > bd) {
#endif
			u32 eitr = InterruptThrottleRate[bd];
			switch (eitr) {
			case 0:
				DPRINTK(PROBE, INFO, "%s turned off\n",
				        opt.name);
				/*
				 * zero is a special value, we don't want to
				 * turn off ITR completely, just set it to an
				 * insane interrupt rate
				 */
				adapter->eitr_param = IXGBE_MAX_INT_RATE;
				adapter->itr_setting = 0;
				break;
			case 1:
				DPRINTK(PROBE, INFO, "dynamic interrupt "
                                        "throttling enabled\n");
				adapter->eitr_param = 20000;
				adapter->itr_setting = 1;
				break;
			default:
				ixgbevf_validate_option(&eitr, &opt);
				adapter->eitr_param = eitr;
				/* the first bit is used as control */
				adapter->itr_setting = eitr & ~1;
				break;
			}
#ifdef module_param_array
		} else {
			adapter->eitr_param = DEFAULT_ITR;
			adapter->itr_setting = DEFAULT_ITR;
		}
#endif
	}
	{ /* Rx buffer mode */
		unsigned int rx_buf_mode;
		static struct ixgbevf_option opt = {
			.type = range_option,
			.name = "Rx buffer mode",
			.err = "using default of "
				__MODULE_STRING(IXGBE_DEFAULT_RXBUFMODE),
			.def = IXGBE_DEFAULT_RXBUFMODE,
			.arg = {.r = {.min = IXGBE_RXBUFMODE_1BUF_ALWAYS,
				      .max = IXGBE_RXBUFMODE_OPTIMAL}}
		};

#ifdef module_param_array
		if (num_RxBufferMode > bd) {
#endif
			rx_buf_mode = RxBufferMode[bd];
			ixgbevf_validate_option(&rx_buf_mode, &opt);
			switch (rx_buf_mode) {
			case IXGBE_RXBUFMODE_OPTIMAL:
				adapter->flags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
				adapter->flags |= IXGBE_FLAG_RX_PS_CAPABLE;
				break;
			case IXGBE_RXBUFMODE_PS_ALWAYS:
				adapter->flags |= IXGBE_FLAG_RX_PS_CAPABLE;
				break;
			case IXGBE_RXBUFMODE_1BUF_ALWAYS:
				adapter->flags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
			default:
				break;
			}
#ifdef module_param_array
		} else {
			adapter->flags |= IXGBE_FLAG_RX_1BUF_CAPABLE;
			adapter->flags |= IXGBE_FLAG_RX_PS_CAPABLE;
		}
#endif
	}
}
