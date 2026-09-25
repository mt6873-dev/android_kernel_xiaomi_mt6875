/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT_RSC_H
#define _MT_RSC_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*
 *   enforce kernel log enable
 */
#define KERNEL_LOG		/* enable debug log flag if defined */

#define _SUPPORT_MAX_RSC_FRAME_REQUEST_ 6
#define _SUPPORT_MAX_RSC_REQUEST_RING_SIZE_ 4


#define SIG_ERESTARTSYS 512	/* ERESTARTSYS */
/*
 *
 */
#define RSC_DEV_MAJOR_NUMBER    251

#define RSC_MAGIC               'r'

#define RSC_REG_RANGE           (0x1000)

#define RSC_BASE_HW   0x1b003000

/*This macro is for setting irq status represnted
 * by a local variable,RSCInfo.IrqInfo.Status[RSC_IRQ_TYPE_INT_RSC_ST]
 */
#define RSC_INT_ST                 (1<<0)


struct RSC_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};

struct RSC_REG_IO_STRUCT {
	struct RSC_REG_STRUCT *pData;	/* pointer to RSC_REG_STRUCT */
	unsigned int Count;	/* count */
};

/*
 *   interrupt clear type
 */
enum RSC_IRQ_CLEAR_ENUM {
	RSC_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	RSC_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
	RSC_IRQ_WAIT_CLEAR,
	/* wait the signal and clear it, avoid hw executime is too s hort. */
	RSC_IRQ_CLEAR_STATUS,	/* clear specific status only */
	RSC_IRQ_CLEAR_ALL	/* clear all status */
};


/*
 *   module's interrupt , each module should have its own isr.
 *   note:
 *	mapping to isr table,ISR_TABLE when using no device tree
 */
enum RSC_IRQ_TYPE_ENUM {
	RSC_IRQ_TYPE_INT_RSC_ST,	/* RSC */
	RSC_IRQ_TYPE_AMOUNT
};

struct RSC_WAIT_IRQ_STRUCT {
	enum RSC_IRQ_CLEAR_ENUM Clear;
	enum RSC_IRQ_TYPE_ENUM Type;
	unsigned int Status;	/*IRQ Status */
	unsigned int Timeout;
	int UserKey;		/* user key for doing interrupt operation */
	int ProcessID;		/* user ProcessID (will filled in kernel) */
	unsigned int bDumpReg;	/* check dump register or not */
};

struct RSC_CLEAR_IRQ_STRUCT {
	enum RSC_IRQ_TYPE_ENUM Type;
	int UserKey;		/* user key for doing interrupt operation */
	unsigned int Status;	/* Input */
};




/*
 * Layout must match the official (A12) kernel / the A12 camerahalserver
 * byte for byte: sizeof(struct RSC_Config) == 0xA8 (168), not 0x70 (112).
 *
 * Official kernel evidence:
 *  - RSC_ioctl (0xffffff8008b003b0) uses 0xa8 as the element size/stride:
 *        __arch_copy_from_user(&uStack_138, param_3, 0xa8)
 *        memcpy(lVar8 + uVar12 * 0xa8 + 0x30, &uStack_138)
 *        memcpy(..., uVar11 * 0x420 + (ulong)uVar29 * 0xa8 + ..., 0xa8)
 *  - CmdqRSCHW (0xffffff8008afda7c) reads the fields it programs into the
 *    RSC registers at these word offsets (write order == A11's register
 *    order INT_CTL, CTRL, SIZE, APLI_C, APLI_P, IMGI_C, IMGI_P, IMGI_C_STRIDE,
 *    IMGI_P_STRIDE, MVI, MVI_STRIDE, MVO, MVO_STRIDE, BVO, BVO_STRIDE,
 *    <13 tunable>, DCM_CTL, START, START):
 *        0x00 CTRL          0x01 SIZE
 *        0x02 IMGI_C_BASE   0x05 IMGI_C_STRIDE   0x06 IMGI_P_BASE   0x09 IMGI_P_STRIDE
 *        0x0A MVI_BASE      0x0D MVI_STRIDE      0x0E APLI_C_BASE   0x11 APLI_P_BASE
 *        0x14 MVO_BASE      0x17 MVO_STRIDE      0x18 BVO_BASE      0x1B BVO_STRIDE
 *        0x1C..0x28 the 13 tunable fields, 0x29 STA_0   (word index)
 *    i.e. every image descriptor is {base, rsv, rsv, stride} (16 bytes),
 *    the two APLI descriptors are {base, rsv, rsv} (12 bytes), and the
 *    13 tunable fields follow contiguously.
 *
 * The 15 reserved words are not consumed by CmdqRSCHW (the RSC only exposes
 * BASE/STRIDE registers per image), but they must stay in place, otherwise
 * every field from IMGI_C_STRIDE on is read from the wrong offset - which is
 * exactly what happened with the old 112-byte layout:
 *    A11 read IMGI_C_STRIDE=0x0C  but the HAL wrote it at 0x14
 *    A11 read APLI_P_BASE =0x24   and got the HAL's IMGI_P_STRIDE (0x120)
 */
struct RSC_Config {
	unsigned int RSC_CTRL;			/* 0x00 */
	unsigned int RSC_SIZE;			/* 0x04 */
	unsigned int RSC_IMGI_C_BASE_ADDR;	/* 0x08 */
	unsigned int RSC_IMGI_C_RSV0;		/* 0x0C */
	unsigned int RSC_IMGI_C_RSV1;		/* 0x10 */
	unsigned int RSC_IMGI_C_STRIDE;		/* 0x14 */
	unsigned int RSC_IMGI_P_BASE_ADDR;	/* 0x18 */
	unsigned int RSC_IMGI_P_RSV0;		/* 0x1C */
	unsigned int RSC_IMGI_P_RSV1;		/* 0x20 */
	unsigned int RSC_IMGI_P_STRIDE;		/* 0x24 */
	unsigned int RSC_MVI_BASE_ADDR;		/* 0x28 */
	unsigned int RSC_MVI_RSV0;		/* 0x2C */
	unsigned int RSC_MVI_RSV1;		/* 0x30 */
	unsigned int RSC_MVI_STRIDE;		/* 0x34 */
	unsigned int RSC_APLI_C_BASE_ADDR;	/* 0x38 */
	unsigned int RSC_APLI_C_RSV0;		/* 0x3C */
	unsigned int RSC_APLI_C_RSV1;		/* 0x40 */
	unsigned int RSC_APLI_P_BASE_ADDR;	/* 0x44 */
	unsigned int RSC_APLI_P_RSV0;		/* 0x48 */
	unsigned int RSC_APLI_P_RSV1;		/* 0x4C */
	unsigned int RSC_MVO_BASE_ADDR;		/* 0x50 */
	unsigned int RSC_MVO_RSV0;		/* 0x54 */
	unsigned int RSC_MVO_RSV1;		/* 0x58 */
	unsigned int RSC_MVO_STRIDE;		/* 0x5C */
	unsigned int RSC_BVO_BASE_ADDR;		/* 0x60 */
	unsigned int RSC_BVO_RSV0;		/* 0x64 */
	unsigned int RSC_BVO_RSV1;		/* 0x68 */
	unsigned int RSC_BVO_STRIDE;		/* 0x6C */
#define RSC_TUNABLE
#ifdef RSC_TUNABLE
	unsigned int RSC_MV_OFFSET;		/* 0x70 */
	unsigned int RSC_GMV_OFFSET;		/* 0x74 */
	unsigned int RSC_CAND_NUM;		/* 0x78 */
	unsigned int RSC_RAND_HORZ_LUT;		/* 0x7C */
	unsigned int RSC_RAND_VERT_LUT;		/* 0x80 */
	unsigned int RSC_SAD_CTRL;		/* 0x84 */
	unsigned int RSC_SAD_EDGE_GAIN_CTRL;	/* 0x88 */
	unsigned int RSC_SAD_CRNR_GAIN_CTRL;	/* 0x8C */
	unsigned int RSC_STILL_STRIP_CTRL0;	/* 0x90 */
	unsigned int RSC_STILL_STRIP_CTRL1;	/* 0x94 */
	unsigned int RSC_RAND_PNLTY_CTRL;	/* 0x98 */
	unsigned int RSC_RAND_PNLTY_GAIN_CTRL0;	/* 0x9C */
	unsigned int RSC_RAND_PNLTY_GAIN_CTRL1;	/* 0xA0 */
#endif
	unsigned int RSC_STA_0;			/* 0xA4 */
};						/* sizeof = 0xA8 */


/*
 *
 */
enum RSC_CMD_ENUM {
	RSC_CMD_RESET,		/* Reset */
	RSC_CMD_DUMP_REG,	/* Dump RSC Register */
	RSC_CMD_DUMP_ISR_LOG,	/* Dump RSC ISR log */
	RSC_CMD_READ_REG,	/* Read register from driver */
	RSC_CMD_WRITE_REG,	/* Write register to driver */
	RSC_CMD_WAIT_IRQ,	/* Wait IRQ */
	RSC_CMD_CLEAR_IRQ,	/* Clear IRQ */
	RSC_CMD_ENQUE_NUM,	/* RSC Enque Number */
	RSC_CMD_ENQUE,		/* RSC Enque */
	RSC_CMD_ENQUE_REQ,	/* RSC Enque Request */
	RSC_CMD_DEQUE_NUM,	/* RSC Deque Number */
	RSC_CMD_DEQUE,		/* RSC Deque */
	RSC_CMD_DEQUE_REQ,	/* RSC Deque Request */
	RSC_CMD_TOTAL,
};
/*  */

struct RSC_Request {
	unsigned int m_ReqNum;
	struct RSC_Config *m_pRscConfig;
};






#ifdef CONFIG_COMPAT
struct compat_RSC_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;	/* count */
};

struct compat_RSC_Request {
	unsigned int m_ReqNum;
	compat_uptr_t m_pRscConfig;
};


#endif




#define RSC_RESET           _IO(RSC_MAGIC, RSC_CMD_RESET)
#define RSC_DUMP_REG        _IO(RSC_MAGIC, RSC_CMD_DUMP_REG)
#define RSC_DUMP_ISR_LOG    _IO(RSC_MAGIC, RSC_CMD_DUMP_ISR_LOG)


#define RSC_READ_REGISTER						\
	_IOWR(RSC_MAGIC, RSC_CMD_READ_REG, struct RSC_REG_IO_STRUCT)
#define RSC_WRITE_REGISTER						\
	_IOWR(RSC_MAGIC, RSC_CMD_WRITE_REG, struct RSC_REG_IO_STRUCT)
#define RSC_WAIT_IRQ							\
	_IOW(RSC_MAGIC, RSC_CMD_WAIT_IRQ, struct RSC_WAIT_IRQ_STRUCT)
#define RSC_CLEAR_IRQ							\
	_IOW(RSC_MAGIC, RSC_CMD_CLEAR_IRQ, struct RSC_CLEAR_IRQ_STRUCT)

#define RSC_ENQNUE_NUM  _IOW(RSC_MAGIC, RSC_CMD_ENQUE_NUM,    int)
#define RSC_ENQUE      _IOWR(RSC_MAGIC, RSC_CMD_ENQUE,      struct RSC_Config)
#define RSC_ENQUE_REQ  _IOWR(RSC_MAGIC, RSC_CMD_ENQUE_REQ,  struct RSC_Request)

#define RSC_DEQUE_NUM  _IOR(RSC_MAGIC, RSC_CMD_DEQUE_NUM,    int)
#define RSC_DEQUE      _IOWR(RSC_MAGIC, RSC_CMD_DEQUE,      struct RSC_Config)
#define RSC_DEQUE_REQ  _IOWR(RSC_MAGIC, RSC_CMD_DEQUE_REQ,  struct RSC_Request)


#ifdef CONFIG_COMPAT
#define COMPAT_RSC_WRITE_REGISTER					\
	_IOWR(RSC_MAGIC, RSC_CMD_WRITE_REG, struct compat_RSC_REG_IO_STRUCT)
#define COMPAT_RSC_READ_REGISTER					\
	_IOWR(RSC_MAGIC, RSC_CMD_READ_REG, struct compat_RSC_REG_IO_STRUCT)

#define COMPAT_RSC_ENQUE_REQ						\
	_IOWR(RSC_MAGIC, RSC_CMD_ENQUE_REQ, struct compat_RSC_Request)
#define COMPAT_RSC_DEQUE_REQ						\
	_IOWR(RSC_MAGIC, RSC_CMD_DEQUE_REQ, struct compat_RSC_Request)
#endif

/*  */
#endif
