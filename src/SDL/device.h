#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED


#include <SDL.h>


extern	int	SCREEN_DX;		/* ������ɥ�����ȡ�		*/
extern	int	SCREEN_DY;		/* ���̥��ꥢ����ȤΥ��ե��å�	*/

extern	int	status_fg;		/* ���ơ��������ʿ�		*/
extern	int	status_bg;		/* ���ơ������طʿ�		*/



extern	int	use_hwsurface;		/* HW SURFACE ��Ȥ����ɤ���	*/
extern	int	use_doublebuf;		/* ���֥�Хåե���Ȥ����ɤ���	*/
extern	int	use_swcursor;		/* ��˥塼�ǥ�������ɽ�����뤫	*/

extern	int	mouse_rel_move;		/* �ޥ������а�ư�̸��β�ǽ��	*/

extern	int	use_cmdkey;		/* Command�����ǥ�˥塼������     */
extern	int	keyboard_type;		/* �����ܡ��ɤμ���                */
extern	char	*file_keyboard;		/* ��������ե�����̾		   */
extern	int	use_joydevice;		/* ���祤���ƥ��å��ǥХ����򳫤�? */



void	sdl_system_init( void );
void	sdl_system_term( void );

int	joy_init( void );

#endif	/* DEVICE_H_INCLUDED */