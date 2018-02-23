/*******************************************************************************

  Intel(R) 10GbE PCI Express Virtual Function Driver
  Copyright(c) 1999 - 2018 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBEVF_OSDEP2_H_
#define _IXGBEVF_OSDEP2_H_

u32 ixgbe_read_reg(struct ixgbe_hw *hw, u32 reg);

static inline void IXGBE_WRITE_REG(struct ixgbe_hw *hw, u32 reg, u32 value)
{
	u8 __iomem *reg_addr;

	reg_addr = READ_ONCE(hw->hw_addr);
	if (IXGBE_REMOVED(reg_addr))
		return;
#ifdef DBG
	switch (reg) {
	case IXGBE_EIMS:
	case IXGBE_EIMC:
	case IXGBE_EIAM:
	case IXGBE_EIAC:
	case IXGBE_EICR:
	case IXGBE_EICS:
		printk("%s: Reg - 0x%05X, value - 0x%08X\n", __func__,
		       reg, value);
	default:
		break;
	}
#endif /* DBG */
	writel(value, reg_addr + reg);
}

static inline void IXGBE_WRITE_REG64(struct ixgbe_hw *hw, u32 reg, u64 value)
{
	u8 __iomem *reg_addr;

	reg_addr = READ_ONCE(hw->hw_addr);
	if (IXGBE_REMOVED(reg_addr))
		return;
	writeq(value, reg_addr + reg);
}

#endif /* _IXGBEVF_OSDEP2_H_ */
