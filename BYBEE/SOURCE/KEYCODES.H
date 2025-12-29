/*
 * keycodes returned from bioskey()
 * rdb 10/4/89
 *
 * Any code with 00 in the low byte is a non-ASCII keycode, and
 * must be handled specially.  If the key has an ASCII equivalent,
 * the scan code will still be in the upper 8 bits, and the ASCII
 * will be in the lower 8.
 */

#define KEY_BS			0x0e08		/* backspace */

#define KEY_F1			0x3b00		/* normal function keys, F1-F10 */
#define KEY_F2			0x3c00
#define KEY_F3			0x3d00
#define KEY_F4			0x3e00
#define KEY_F5			0x3f00
#define KEY_F6			0x4000
#define KEY_F7			0x4100
#define KEY_F8			0x4200
#define KEY_F9			0x4300
#define KEY_F10			0x4400

#define KEY_HOME		0x4700
#define KEY_UP			0x4800
#define KEY_PGUP		0x4900
#define KEY_MINUSPAD	0x4a00
#define KEY_LEFT		0x4b00
#define KEY_RIGHT		0x4d00
#define KEY_PLUSPAD		0x4e00
#define KEY_END			0x4f00
#define KEY_DOWN		0x5000
#define KEY_PGDN		0x5100

#define KEY_INSERT		0x5200
#define KEY_DELETE		0x5300

#define KEY_SF1			0x5400		/* shifted F1-F10 */
#define KEY_SF2			0x5500
#define KEY_SF3			0x5600
#define KEY_SF4			0x5700
#define KEY_SF5			0x5800
#define KEY_SF6			0x5900
#define KEY_SF7			0x5a00
#define KEY_SF8			0x5b00
#define KEY_SF9			0x5c00
#define KEY_SF10		0x5d00

#define KEY_CF1			0x5e00		/* control F1-F10 */
#define KEY_CF2			0x5f00
#define KEY_CF3			0x6000
#define KEY_CF4			0x6100
#define KEY_CF5			0x6200
#define KEY_CF6			0x6300
#define KEY_CF7			0x6400
#define KEY_CF8			0x6500
#define KEY_CF9			0x6600
#define KEY_CF10		0x6700

#define KEY_AF1			0x6800		/* ALT F1-F10 */
#define KEY_AF2			0x6900
#define KEY_AF3			0x6a00
#define KEY_AF4			0x6b00
#define KEY_AF5			0x6c00
#define KEY_AF6			0x6d00
#define KEY_AF7			0x6e00
#define KEY_AF8			0x6f00
#define KEY_AF9			0x7000
#define KEY_AF10		0x7100

#define KEY_CPRTSC		0x7200		/* ctrl- print-screen */
#define KEY_CLEFT		0x7300
#define KEY_CRIGHT		0x7400
#define KEY_CEND		0x7500
#define KEY_CPGDN		0x7600
#define KEY_CHOME		0x7700

#define KEY_ALT1		0x7800
#define KEY_ALT2		0x7900
#define KEY_ALT3		0x7a00
#define KEY_ALT4		0x7b00
#define KEY_ALT5		0x7c00
#define KEY_ALT6		0x7d00
#define KEY_ALT7		0x7e00
#define KEY_ALT8		0x7f00
#define KEY_ALT9		0x8000
#define KEY_ALT0		0x8100
#define KEY_ALTMINUS	0x8200
#define KEY_ALTEQUALS	0x8300

#define KEY_CPGUP		0x8400
