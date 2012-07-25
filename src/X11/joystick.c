/************************************************************************/
/*									*/
/* ジョイスティック入力							*/
/*									*/
/************************************************************************/

/*
  現在のところ、イベント処理側で処理できるジョイスティックの要件は、

	上下左右方向  1個
	ボタン	      8個  (ただし、PC-8801に伝わるのは2個)

	ジョイスティックそのものは、 1個 
*/

#include "joy-none.c"

#include "joy-linux.c"

#include "joy-bsd.c"

#include "joy-sdl.c"

