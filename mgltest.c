#include <stdarg.h>
#include "mgraph.h"
#include "pmapi.h"

font_t *system_font;
char *msgbuf, showscore;
MGLDC   *dc;

#define CHECKPOINT(n) // { blankline(0); mprintf(0,0,MGL_WHITE,"Checkpoint #%d", n); PM_sleep(2000);}

struct coord4d {
	char x, y, z, t;
};

struct a_board {
	char board[3][3][3][3];
} boardstatus, aiboard_offense, aiboard_defense;

void mprintf( int col, int row, char clr, char *fmt, ... ) {
	va_list args;

	va_start( args, fmt );
	vsprintf( msgbuf, fmt, args );
	va_end( args );
	MGL_setColorCI(clr);
	MGL_drawStrXY( col*9, row*9, msgbuf );
}

char yesno( int col, int row, char clr, char *question ) {
	event_t evt;

	mprintf( col, row, clr, "%s", question );

checkyesno:
	EVT_halt(&evt,EVT_KEYDOWN);
	if ( EVT_asciiCode( evt.message ) != 'Y' && EVT_asciiCode( evt.message ) != 'N' && 
	 EVT_asciiCode( evt.message ) != 'y' && EVT_asciiCode( evt.message ) != 'n' )
	  goto checkyesno;
	if ( EVT_asciiCode( evt.message ) == 'Y' || EVT_asciiCode( evt.message ) == 'y' )
	  return 1;
	return 0;
}

void blankline( int line ) {
	MGL_setColorCI(MGL_BLACK);
	MGL_fillRectCoord( 0, line*9, 639, (line*9)+8 );
}

char getcoord( char *prompt ) {
	event_t evt;

	mprintf( 0, 52, MGL_GREEN, "%s", prompt );

getnum:
	EVT_halt(&evt,EVT_KEYDOWN);
	if ( EVT_asciiCode( evt.message ) == 0x1b ) return 4;
	if ( EVT_asciiCode( evt.message ) < '1' || EVT_asciiCode( evt.message ) > '3' )
	  goto getnum;
	return EVT_asciiCode( evt.message ) - '1';
}

char getcoords( char *x, char *y, char *z, char *t ) {
	blankline( 52 );
	*x = getcoord( "Type the X (column) coordinate of your move (1-3):" );
	if ( *x == 4 ) return 1;
	blankline( 52 );
	*y = getcoord( "Type the Y (row) coordinate of your move (1-3):" );
	if ( *y == 4 ) return 1;
	blankline( 52 );
	*z = getcoord( "Type the Z (height) coordinate of your move (1-3):" );
	if ( *z == 4 ) return 1;
	blankline( 52 );
	*t = getcoord( "Type the T (time represented by large columns) coordinate of your move (1-3):" );
	if ( *t == 4 ) return 1;
	blankline( 52 );
	return 0;
}

#define TTT_UNOCCUPIED	0
#define TTT_X_OWNS	1
#define TTT_O_OWNS	2
#define TTT_NEUTRAL	3
#define TTT_OLD_X_OWNS	4
#define TTT_OLD_O_OWNS	5

#define TEMP_COLOR	32 // index away from the standard 16

point_t basepoly[4] = {
	{ 20, 0 }, { 56, 0 }, { 36, 36 }, { 0, 36 }
};
point_t boardpoly[5];

int xscore = 0, oscore = 0;
char noquit = 1, whoseturn = 0;

void placemark( char x, char y, char z, char t, char marktype ) {
	int i;
	unsigned char hilite, fillcol, or, og, ob, fadedown = 0;

	CHECKPOINT(100);
	switch( marktype ) {
		case TTT_UNOCCUPIED:
			hilite = MGL_WHITE;	fillcol = MGL_BLACK;		fadedown=1;
		break;
		case TTT_X_OWNS:
			hilite = MGL_LIGHTBLUE;	fillcol = MGL_LIGHTBLUE;
		break;
		case TTT_O_OWNS:
			hilite = MGL_LIGHTCYAN;	fillcol = MGL_LIGHTCYAN;
		break;
		case TTT_OLD_X_OWNS:
			hilite = MGL_LIGHTBLUE;	fillcol = MGL_BLUE;		fadedown=1;
		break;
		case TTT_OLD_O_OWNS:
			hilite = MGL_LIGHTCYAN;	fillcol = MGL_CYAN;		fadedown=1;
		break;
		case TTT_NEUTRAL:
			hilite = MGL_LIGHTGRAY;	fillcol = MGL_DARKGRAY;
		break;
	}
	
	if ( !fadedown ) MGL_getPaletteEntry( dc, fillcol, &or, &og, &ob ); else
	  MGL_getPaletteEntry( dc, hilite, &or, &og, &ob );

	if ( !fadedown ) {
		MGL_setPaletteEntry( dc, TEMP_COLOR, 0, 0, 0 );
		MGL_realizePalette( dc, 1, TEMP_COLOR, 1 );
	} else {
		MGL_setPaletteEntry( dc, TEMP_COLOR, or, og, ob );
		MGL_realizePalette( dc, 1, TEMP_COLOR, 1 );
	}

	MGL_setColorCI( TEMP_COLOR );
	MGL_fillPolygon(4, basepoly, 54+(x*36)-(y*20)+(t*214), 36+(y*36)+(z*150) );

	if ( !fadedown ) {
		MGL_setPaletteEntry( dc, TEMP_COLOR, 0, 0, 0 );
		MGL_realizePalette( dc, 1, TEMP_COLOR, 1 );
		for ( i=0; i<50; ++i ) {
			MGL_setPaletteEntry( dc, TEMP_COLOR, (or*i)/50, (og*i)/50, (ob*i)/50 );
			MGL_realizePalette( dc, 1, TEMP_COLOR, 1 );
		}
	} else {
		MGL_setPaletteEntry( dc, TEMP_COLOR, or, og, ob );
		MGL_realizePalette( dc, 1, TEMP_COLOR, 1 );
		for ( i=25; i>=0; --i ) {
			MGL_setPaletteEntry( dc, TEMP_COLOR, (or*(i+25))/50, (og*(i+25))/50, (ob*(i+25))/50 );
			MGL_realizePalette( dc, 1, TEMP_COLOR, 1 );
		}
	}

	MGL_setColorCI( fillcol );
	MGL_fillPolygon(4, basepoly, 54+(x*36)-(y*20)+(t*214), 36+(y*36)+(z*150) );

	for (i=0; i<4; ++i) {
		boardpoly[i].x = basepoly[i].x + 54+(x*36)-(y*20)+(t*214);
		boardpoly[i].y = basepoly[i].y + 36+(y*36)+(z*150);
	}
	boardpoly[4].x = boardpoly[0].x;
	boardpoly[4].y = boardpoly[0].y;
	MGL_setColorCI( hilite );
	MGL_polyLine( 5, boardpoly );
	CHECKPOINT(101);
	boardstatus.board[t][z][y][x] = marktype;
	CHECKPOINT(102);
}

void score_me( int x, int y, int z, int t ) {
	int i,j,k,mx,my,mz,mt;
	char hilite;
	for (i=0; i<3; ++i) {
		j=2-i;
		if (x==4) mx=i; else if (x==5) mx=j; else mx=x;
		if (y==4) my=i; else if (y==5) my=j; else my=y;
		if (z==4) mz=i; else if (z==5) mz=j; else mz=z;
		if (t==4) mt=i; else if (t==5) mt=j; else mt=t;
		MGL_setColorCI( MGL_YELLOW );
		MGL_fillPolygon(4, basepoly, 54+(mx*36)-(my*20)+(mt*214), 36+(my*36)+(mz*150) );
	}
	PM_sleep(1000);
	for (i=0; i<3; ++i) {
		j=2-i;
		if (x==4) mx=i; else if (x==5) mx=j; else mx=x;
		if (y==4) my=i; else if (y==5) my=j; else my=y;
		if (z==4) mz=i; else if (z==5) mz=j; else mz=z;
		if (t==4) mt=i; else if (t==5) mt=j; else mt=t;
		switch ( boardstatus.board[mt][mz][my][mx] ) {
			case TTT_OLD_X_OWNS:	MGL_setColorCI( MGL_BLUE ); hilite = MGL_LIGHTBLUE; break;
			case TTT_OLD_O_OWNS:	MGL_setColorCI( MGL_CYAN ); hilite = MGL_LIGHTCYAN; break;
			case TTT_X_OWNS:	MGL_setColorCI( MGL_LIGHTBLUE ); hilite = MGL_LIGHTBLUE; break;
			case TTT_O_OWNS:	MGL_setColorCI( MGL_LIGHTCYAN ); hilite = MGL_LIGHTCYAN; break;
			case TTT_NEUTRAL:	MGL_setColorCI( MGL_DARKGRAY ); hilite = MGL_LIGHTGRAY; break;
		}
		MGL_fillPolygon(4, basepoly, 54+(mx*36)-(my*20)+(mt*214), 36+(my*36)+(mz*150) );
		for (k=0; k<4; ++k) {
			boardpoly[k].x = basepoly[k].x + 54+(mx*36)-(my*20)+(mt*214);
			boardpoly[k].y = basepoly[k].y + 36+(my*36)+(mz*150);
		}
		boardpoly[4].x = boardpoly[0].x;
		boardpoly[4].y = boardpoly[0].y;
		MGL_setColorCI( hilite );
		MGL_polyLine( 5, boardpoly );
	}
}

#define SCORE( x, y, z, t ) {					\
	tempscore++;						\
	if ( showscore ) score_me( x, y, z, t );		\
}

int checkscore( char x, char y, char z, char t, struct a_board *theboard ) {
	signed char i,j;
	unsigned char mine, mineold, gotya;
	int tempscore=0;

	if ( whoseturn ) {
		mine = TTT_O_OWNS;
		mineold = TTT_OLD_O_OWNS;
	} else {
		mine = TTT_X_OWNS;
		mineold = TTT_OLD_X_OWNS;
	}

	CHECKPOINT(0);
	// hold X,Y,Z constant, advance T
	gotya = 0;
	for ( i=0; i<3; ++i ) {
		if ( theboard->board[i][z][y][x] == mine ||
		  theboard->board[i][z][y][x] == TTT_NEUTRAL ||
		  theboard->board[i][z][y][x] == mineold ) {
			gotya++;
		}
	}
	if ( gotya == 3 ) SCORE(x,y,z,4);

	CHECKPOINT(1);
	// hold X,Y,T constant, advance Z
	gotya = 0;
	for ( i=0; i<3; ++i ) {
		if ( theboard->board[t][i][y][x] == mine ||
		  theboard->board[t][i][y][x] == TTT_NEUTRAL ||
		  theboard->board[t][i][y][x] == mineold ) {
			gotya++;
		}
	}
	if ( gotya == 3 ) SCORE(x,y,4,t);

	CHECKPOINT(2);
	// hold X,Z,T constant, advance Y
	gotya = 0;
	for ( i=0; i<3; ++i ) {
		if ( theboard->board[t][z][i][x] == mine ||
		  theboard->board[t][z][i][x] == TTT_NEUTRAL ||
		  theboard->board[t][z][i][x] == mineold ) {
			gotya++;
		}
	}
	if ( gotya == 3 ) SCORE(x,4,z,t);

	CHECKPOINT(3);
	// hold Y,Z,T constant, advance X
	gotya = 0;
	for ( i=0; i<3; ++i ) {
		if ( theboard->board[t][z][y][i] == mine ||
		  theboard->board[t][z][y][i] == TTT_NEUTRAL ||
		  theboard->board[t][z][y][i] == mineold ) {
			gotya++;
		}
	}
	if ( gotya == 3 ) SCORE(4,y,z,t);

	CHECKPOINT(4);
	// hold Z,T constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( (y == 0 || y == 1) && x==y ) {
			// advance Y
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				if ( theboard->board[t][z][i][i] == mine ||
				  theboard->board[t][z][i][i] == TTT_NEUTRAL ||
				  theboard->board[t][z][i][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(4,4,z,t);
		}
		if ( (y == 1 || y == 2) && x==2-y ) {
			// advance Y backwards
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				j=2-i;
				if ( theboard->board[t][z][j][i] == mine ||
				  theboard->board[t][z][j][i] == TTT_NEUTRAL ||
				  theboard->board[t][z][j][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(4,5,z,t);
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( (y == 0 || y == 1) && x==2-y ) {
			// advance Y
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				j=2-i;
				if ( theboard->board[t][z][i][j] == mine ||
				  theboard->board[t][z][i][j] == TTT_NEUTRAL ||
				  theboard->board[t][z][i][j] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(5,4,z,t);
		}
		if ( (y == 1 || y == 2) && x==y ) {
			// advance Y backwards
			gotya = 0;
			for ( i=2; i>=0; --i ) {
				if ( theboard->board[t][z][i][i] == mine ||
				  theboard->board[t][z][i][i] == TTT_NEUTRAL ||
				  theboard->board[t][z][i][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(5,5,z,t);
		}
	}

	CHECKPOINT(5);
	// hold Z,Y constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( (t == 0 || t == 1) && t==x ) {
			// advance T
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				if ( theboard->board[i][z][y][i] == mine ||
				  theboard->board[i][z][y][i] == TTT_NEUTRAL ||
				  theboard->board[i][z][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(4,y,z,4);
		}
		if ( (t == 1 || t == 2) && x==2-t ) {
			// advance T backwards
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				j=2-i;
				if ( theboard->board[j][z][y][i] == mine ||
				  theboard->board[j][z][y][i] == TTT_NEUTRAL ||
				  theboard->board[j][z][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(4,y,z,5);
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( (t == 0 || t == 1) && x==2-t ) {
			// advance T
			gotya = 0;
			for ( j=0; j<3; ++j ) {
				i=2-j;
				if ( theboard->board[j][z][y][i] == mine ||
				  theboard->board[j][z][y][i] == TTT_NEUTRAL ||
				  theboard->board[j][z][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(5,y,z,4);
		}
		if ( (t == 1 || t == 2) && x==t ) {
			// advance T backwards
			gotya = 0;
			for ( i=2; i>=0; --i ) {
				if ( theboard->board[i][z][y][i] == mine ||
				  theboard->board[i][z][y][i] == TTT_NEUTRAL ||
				  theboard->board[i][z][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(5,y,z,5);
		}
	}

	CHECKPOINT(6);
	// hold Z,X constant
	if ( t == 0 || t == 1 ) {
		// advance T
		if ( (y == 0 || y == 1) && y==t ) {
			// advance Y
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				if ( theboard->board[i][z][i][x] == mine ||
				  theboard->board[i][z][i][x] == TTT_NEUTRAL ||
				  theboard->board[i][z][i][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,4,z,4);
		}
		if ( (y == 1 || y == 2) && y==2-t ) {
			// advance Y backwards
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				j=2-i;
				if ( theboard->board[i][z][j][x] == mine ||
				  theboard->board[i][z][j][x] == TTT_NEUTRAL ||
				  theboard->board[i][z][j][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,5,z,4);
		}
	}
	if ( t == 2 ) {
		// advance T backwards
		if ( (y == 0 || y == 1) && y==2-t ) {
			// advance Y
			gotya = 0;
			for ( j=0; j<3; ++j ) {
				i=2-j;
				if ( theboard->board[i][z][j][x] == mine ||
				  theboard->board[i][z][j][x] == TTT_NEUTRAL ||
				  theboard->board[i][z][j][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,4,z,5);
		}
		if ( (y == 1 || y == 2) && y==t ) {
			// advance Y backwards
			gotya = 0;
			for ( i=2; i>=0; --i ) {
				if ( theboard->board[i][z][i][x] == mine ||
				  theboard->board[i][z][i][x] == TTT_NEUTRAL ||
				  theboard->board[i][z][i][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,5,z,5);
		}
	}

	CHECKPOINT(7);
	// hold X,Y constant
	if ( t == 0 || t == 1 ) {
		// advance T
		if ( (z == 0 || z == 1) && z==t ) {
			// advance Z
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				if ( theboard->board[i][i][y][x] == mine ||
				  theboard->board[i][i][y][x] == TTT_NEUTRAL ||
				  theboard->board[i][i][y][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,y,4,4);
		}
		if ( (z == 1 || z == 2) && z==2-t ) {
			// advance Z backwards
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				j=2-i;
				if ( theboard->board[i][j][y][x] == mine ||
				  theboard->board[i][j][y][x] == TTT_NEUTRAL ||
				  theboard->board[i][j][y][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,y,5,4);
		}
	}
	if ( t == 2 ) {
		// advance T backwards
		if ( (z == 0 || z == 1) && z==2-t) {
			// advance Z
			gotya = 0;
			for ( j=0; j<3; ++j ) {
				i=2-j;
				if ( theboard->board[i][j][y][x] == mine ||
				  theboard->board[i][j][y][x] == TTT_NEUTRAL ||
				  theboard->board[i][j][y][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,y,4,5);
		}
		if ( (z == 1 || z == 2) && z==t ) {
			// advance Z backwards
			gotya = 0;
			for ( i=2; i>=0; --i ) {
				if ( theboard->board[i][i][y][x] == mine ||
				  theboard->board[i][i][y][x] == TTT_NEUTRAL ||
				  theboard->board[i][i][y][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,y,5,5);
		}
	}

	CHECKPOINT(8);
	// hold X,T constant
	if ( y == 0 || y == 1 ) {
		// advance Y
		if ( (z == 0 || z == 1) && y==z ) {
			// advance Z
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				if ( theboard->board[t][i][i][x] == mine ||
				  theboard->board[t][i][i][x] == TTT_NEUTRAL ||
				  theboard->board[t][i][i][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,4,4,t);
		}
		if ( (z == 1 || z == 2) && y==2-z ) {
			// advance Z backwards
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				j=2-i;
				if ( theboard->board[t][j][i][x] == mine ||
				  theboard->board[t][j][i][x] == TTT_NEUTRAL ||
				  theboard->board[t][j][i][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,4,5,t);
		}
	}
	if ( y == 2 ) {
		// advance Y backwards
		if ( (z == 0 || z == 1) && y==2-z ) {
			// advance Z
			gotya = 0;
			for ( j=0; j<3; ++j ) {
				i=2-j;
				if ( theboard->board[t][j][i][x] == mine ||
				  theboard->board[t][j][i][x] == TTT_NEUTRAL ||
				  theboard->board[t][j][i][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,5,4,t);
		}
		if ( (z == 1 || z == 2) && y==z ) {
			// advance Z backwards
			gotya = 0;
			for ( i=2; i>=0; --i ) {
				if ( theboard->board[t][i][i][x] == mine ||
				  theboard->board[t][i][i][x] == TTT_NEUTRAL ||
				  theboard->board[t][i][i][x] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(x,5,5,t);
		}
	}

	CHECKPOINT(9);
	// hold Y,T constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( (z == 0 || z == 1) && x==z ) {
			// advance Z
			gotya = 0;
			for ( i=0; i<3; ++i ) {
				if ( theboard->board[t][i][y][i] == mine ||
				  theboard->board[t][i][y][i] == TTT_NEUTRAL ||
				  theboard->board[t][i][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(4,y,4,t);
		}
		if ( (z == 1 || z == 2) && x==2-z ) {
			// advance Z backwards
			gotya = 0;
			for ( j=2; j>=0; --j ) {
				i=2-j;
				if ( theboard->board[t][j][y][i] == mine ||
				  theboard->board[t][j][y][i] == TTT_NEUTRAL ||
				  theboard->board[t][j][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(4,y,5,t);
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( (z == 0 || z == 1) && x==2-z ) {
			// advance Z
			gotya = 0;
			for ( j=0; j<3; ++j ) {
				i=2-j;
				if ( theboard->board[t][j][y][i] == mine ||
				  theboard->board[t][j][y][i] == TTT_NEUTRAL ||
				  theboard->board[t][j][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(5,y,4,t);
		}
		if ( (z == 1 || z == 2) && x==z ) {
			// advance Z backwards
			gotya = 0;
			for ( i=2; i>=0; --i ) {
				if ( theboard->board[t][i][y][i] == mine ||
				  theboard->board[t][i][y][i] == TTT_NEUTRAL ||
				  theboard->board[t][i][y][i] == mineold ) {
					gotya++;
				}
			}
			if ( gotya == 3 ) SCORE(5,y,5,t);
		}
	}

	CHECKPOINT(10);
	// hold X constant
	if ( y == 0 || y == 1 ) {
		// advance Y
		if ( z == 0 || z == 1 ) {
			// advance Z
			if ( (t == 0 || t == 1) && y==z && y==t ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					if ( theboard->board[i][i][i][x] == mine ||
					  theboard->board[i][i][i][x] == TTT_NEUTRAL ||
					  theboard->board[i][i][i][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,4,4,4);
			}
			if ( (t == 1 || t == 2) && y==z && t==2-y ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][i][i][x] == mine ||
					  theboard->board[j][i][i][x] == TTT_NEUTRAL ||
					  theboard->board[j][i][i][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,4,4,5);
			}
		}
		if ( z == 1 || z == 2 ) {
			// advance Z backwards
			if ( (t == 0 || t == 1) && y==t && z==2-y ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][j][i][x] == mine ||
					  theboard->board[i][j][i][x] == TTT_NEUTRAL ||
					  theboard->board[i][j][i][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,4,5,4);
			}
			if ( (t == 1 || t == 2) && y==2-z && z==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][j][i][x] == mine ||
					  theboard->board[j][j][i][x] == TTT_NEUTRAL ||
					  theboard->board[j][j][i][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,4,5,5);
			}
		}
	}
	if ( y == 2 ) {
		// advance Y backwards
		if ( z == 0 || z == 1 ) {
			// advance Z
			if ( (t == 0 || t == 1) && y==2-z && z==t ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][i][j][x] == mine ||
					  theboard->board[i][i][j][x] == TTT_NEUTRAL ||
					  theboard->board[i][i][j][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,5,4,4);
			}
			if ( (t == 1 || t == 2) && z==2-y && y==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][i][j][x] == mine ||
					  theboard->board[j][i][j][x] == TTT_NEUTRAL ||
					  theboard->board[j][i][j][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,5,4,5);
			}
		}
		if ( z == 1 || z == 2 ) {
			// advance Z backwards
			if ( (t == 0 || t == 1) && y==z && z==2-t ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][j][j][x] == mine ||
					  theboard->board[i][j][j][x] == TTT_NEUTRAL ||
					  theboard->board[i][j][j][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,5,5,4);
			}
			if ( (t == 1 || t == 2 ) && y==z && z==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=2; i>=0; --i ) {
					if ( theboard->board[i][i][i][x] == mine ||
					  theboard->board[i][i][i][x] == TTT_NEUTRAL ||
					  theboard->board[i][i][i][x] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(x,5,5,5);
			}
		}
	}

	CHECKPOINT(11);
	// hold Y constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( z == 0 || z == 1 ) {
			// advance Z
			if ( (t == 0 || t == 1) && x==z && x==t) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					if ( theboard->board[i][i][y][i] == mine ||
					  theboard->board[i][i][y][i] == TTT_NEUTRAL ||
					  theboard->board[i][i][y][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,y,4,4);
			}
			if ( (t == 1 || t == 2) && x==z && x==2-t ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][i][y][i] == mine ||
					  theboard->board[j][i][y][i] == TTT_NEUTRAL ||
					  theboard->board[j][i][y][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,y,4,5);
			}
		}
		if ( z == 1 || z == 2 ) {
			// advance Z backwards
			if ( (t == 0 || t == 1) && x==t && x==2-z ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][j][y][i] == mine ||
					  theboard->board[i][j][y][i] == TTT_NEUTRAL ||
					  theboard->board[i][j][y][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,y,5,4);
			}
			if ( (t == 1 || t == 2) && x==2-z && z==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][j][y][i] == mine ||
					  theboard->board[j][j][y][i] == TTT_NEUTRAL ||
					  theboard->board[j][j][y][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,y,5,5);
			}
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( z == 0 || z == 1 ) {
			// advance Z
			if ( (t == 0 || t == 1) && x==2-z && z==t ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][i][y][j] == mine ||
					  theboard->board[i][i][y][j] == TTT_NEUTRAL ||
					  theboard->board[i][i][y][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,y,4,4);
			}
			if ( (t == 1 || t == 2) && x==t && z==2-x ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][i][y][j] == mine ||
					  theboard->board[j][i][y][j] == TTT_NEUTRAL ||
					  theboard->board[j][i][y][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,y,4,5);
			}
		}
		if ( z == 1 || z == 2 ) {
			// advance Z backwards
			if ( (t == 0 || t == 1) && x==z && t==2-z ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][j][y][j] == mine ||
					  theboard->board[i][j][y][j] == TTT_NEUTRAL ||
					  theboard->board[i][j][y][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,y,5,4);
			}
			if ( (t == 1 || t == 2) && x==t && z==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=2; i>=0; --i ) {
					if ( theboard->board[i][i][y][i] == mine ||
					  theboard->board[i][i][y][i] == TTT_NEUTRAL ||
					  theboard->board[i][i][y][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,y,5,5);
			}
		}
	}

	CHECKPOINT(12);
	// hold Z constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( y == 0 || y == 1 ) {
			// advance Y
			if ( (t == 0 || t == 1) && x==y && x==t ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					if ( theboard->board[i][z][i][i] == mine ||
					  theboard->board[i][z][i][i] == TTT_NEUTRAL ||
					  theboard->board[i][z][i][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,4,z,4);
			}
			if ( (t == 1 || t == 2) && x==y && t==2-y ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][z][i][i] == mine ||
					  theboard->board[j][z][i][i] == TTT_NEUTRAL ||
					  theboard->board[j][z][i][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,4,z,5);
			}
		}
		if ( y == 1 || y == 2 ) {
			// advance Y backwards
			if ( (t == 0 || t == 1) && x==t && y==2-x ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][z][j][i] == mine ||
					  theboard->board[i][z][j][i] == TTT_NEUTRAL ||
					  theboard->board[i][z][j][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,5,z,4);
			}
			if ( (t == 1 || t == 2) && x==2-y && y==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][z][j][i] == mine ||
					  theboard->board[j][z][j][i] == TTT_NEUTRAL ||
					  theboard->board[j][z][j][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,5,z,5);
			}
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( y == 0 || y == 1 ) {
			// advance Y
			if ( (t == 0 || t == 1) && y==t && x==2-y ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][z][i][j] == mine ||
					  theboard->board[i][z][i][j] == TTT_NEUTRAL ||
					  theboard->board[i][z][i][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,4,z,4);
			}
			if ( (t == 1 || t == 2) && x==t && x==2-y ) {
				// advance T backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[j][z][i][j] == mine ||
					  theboard->board[j][z][i][j] == TTT_NEUTRAL ||
					  theboard->board[j][z][i][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,4,z,5);
			}
		}
		if ( y == 1 || y == 2 ) {
			// advance Y backwards
			if ( (t == 0 || t == 1) && x==y && t==2-x ) {
				// advance T
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[i][z][j][j] == mine ||
					  theboard->board[i][z][j][j] == TTT_NEUTRAL ||
					  theboard->board[i][z][j][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,5,z,4);
			}
			if ( (t == 1 || t == 2) && x==y && x==t ) {
				// advance T backwards
				gotya = 0;
				for ( i=2; i>=0; --i ) {
					if ( theboard->board[i][z][i][i] == mine ||
					  theboard->board[i][z][i][i] == TTT_NEUTRAL ||
					  theboard->board[i][z][i][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,5,z,5);
			}
		}
	}

	CHECKPOINT(13);
	// hold T constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( y == 0 || y == 1 ) {
			// advance Y
			if ( (z == 0 || z == 1) && x==y && x==z ) {
				// advance Z
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					if ( theboard->board[t][i][i][i] == mine ||
					  theboard->board[t][i][i][i] == TTT_NEUTRAL ||
					  theboard->board[t][i][i][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,4,4,t);
			}
			if ( (z == 1 || z == 2) && x==y && z==2-y ) {
				// advance Z backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[t][j][i][i] == mine ||
					  theboard->board[t][j][i][i] == TTT_NEUTRAL ||
					  theboard->board[t][j][i][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,4,5,t);
			}
		}
		if ( y == 1 || y == 2 ) {
			// advance Y backwards
			if ( (z == 0 || z == 1) && x==z && y==2-x ) {
				// advance Z
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[t][i][j][i] == mine ||
					  theboard->board[t][i][j][i] == TTT_NEUTRAL ||
					  theboard->board[t][i][j][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,5,4,t);
			}
			if ( (z == 1 || z == 2) && x==2-y && y==z ) {
				// advance Z backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[t][j][j][i] == mine ||
					  theboard->board[t][j][j][i] == TTT_NEUTRAL ||
					  theboard->board[t][j][j][i] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(4,5,5,t);
			}
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( y == 0 || y == 1 ) {
			// advance Y
			if ( (z == 0 || z == 1) && x==2-y && y==z ) {
				// advance Z
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[t][i][i][j] == mine ||
					  theboard->board[t][i][i][j] == TTT_NEUTRAL ||
					  theboard->board[t][i][i][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,4,4,t);
			}
			if ( (z == 1 || z == 2) && x==z && y==2-x ) {
				// advance Z backwards
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[t][j][i][j] == mine ||
					  theboard->board[t][j][i][j] == TTT_NEUTRAL ||
					  theboard->board[t][j][i][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,4,5,t);
			}
		}
		if ( y == 1 || y == 2 ) {
			// advance Y backwards
			if ( (z == 0 || z == 1) && x==y && z==2-x ) {
				// advance Z
				gotya = 0;
				for ( i=0; i<3; ++i ) {
					j=2-i;
					if ( theboard->board[t][i][j][j] == mine ||
					  theboard->board[t][i][j][j] == TTT_NEUTRAL ||
					  theboard->board[t][i][j][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,5,4,t);
			}
			if ( (z == 1 || z == 2) && x==y && x==z ) {
				// advance z backwards
				gotya = 0;
				for ( j=2; j>=0; --j ) {
					if ( theboard->board[t][j][j][j] == mine ||
					  theboard->board[t][j][j][j] == TTT_NEUTRAL ||
					  theboard->board[t][j][j][j] == mineold ) {
						gotya++;
					}
				}
				if ( gotya == 3 ) SCORE(5,5,5,t);
			}
		}
	}

	CHECKPOINT(14);
	// hold NOTHING constant
	if ( x == 0 || x == 1 ) {
		// advance X
		if ( y == 0 || y == 1 ) {
			// advance Y
			if ( z == 0 || z == 1 ) {
				// advance Z
				if ( (t == 0 || t == 1) && x==y && x==z && x==t ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						if ( theboard->board[i][i][i][i] == mine ||
						  theboard->board[i][i][i][i] == TTT_NEUTRAL ||
						  theboard->board[i][i][i][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,4,4,4);
				}
				if ( (t == 1 || t == 2) && x==y && x==z && x==2-t ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][i][i][i] == mine ||
						  theboard->board[j][i][i][i] == TTT_NEUTRAL ||
						  theboard->board[j][i][i][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,4,4,5);
				}
			}
			if ( z == 1 || z == 2 ) {
				// advance Z backwards
				if ( (t == 0 || t == 1) && x==y && t==y && z==2-y ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][j][i][i] == mine ||
						  theboard->board[i][j][i][i] == TTT_NEUTRAL ||
						  theboard->board[i][j][i][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,4,5,4);
				}
				if ( (t == 1 || t == 2) && x==y && t==z && z==2-y ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][j][i][i] == mine ||
						  theboard->board[j][j][i][i] == TTT_NEUTRAL ||
						  theboard->board[j][j][i][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,4,5,5);
				}
			}
		}
		if ( y == 1 || y == 2 ) {
			// advance Y backwards
			if ( z == 0 || z == 1 ) {
				// advance Z
				if ( (t == 0 || t == 1) && x==z && t==z && y==2-z ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][i][j][i] == mine ||
						  theboard->board[i][i][j][i] == TTT_NEUTRAL ||
						  theboard->board[i][i][j][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,5,4,4);
				}
				if ( (t == 1 || t == 2) && x==z && t==y && y==2-z ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][i][j][i] == mine ||
						  theboard->board[j][i][j][i] == TTT_NEUTRAL ||
						  theboard->board[j][i][j][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,5,4,5);
				}
			}
			if ( z == 1 || z == 2 ) {
				// advance Z backwards
				if ( (t == 0 || t == 1) && y==z && x==t && x==2-z ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][j][j][i] == mine ||
						  theboard->board[i][j][j][i] == TTT_NEUTRAL ||
						  theboard->board[i][j][j][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,5,5,4);
				}
				if ( (t == 1 || t == 2) && y==z && t==z && x==2-y ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][j][j][i] == mine ||
						  theboard->board[j][j][j][i] == TTT_NEUTRAL ||
						  theboard->board[j][j][j][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(4,5,5,5);
				}
			}
		}
	}
	if ( x == 2 ) {
		// advance X backwards
		if ( y == 0 || y == 1 ) {
			// advance Y
			if ( z == 0 || z == 1 ) {
				// advance Z
				if ( (t == 0 || t == 1) && x==2-y && y==z && y==t ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][i][i][j] == mine ||
						  theboard->board[i][i][i][j] == TTT_NEUTRAL ||
						  theboard->board[i][i][i][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,4,4,4);
				}
				if ( (t == 1 || t == 2) && x==t && y==z && y==2-x ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][i][i][j] == mine ||
						  theboard->board[j][i][i][j] == TTT_NEUTRAL ||
						  theboard->board[j][i][i][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,4,4,5);
				}
			}
			if ( z == 1 || z == 2 ) {
				// advance Z backwards
				if ( (t == 0 || t == 1) && x==z && t==y && z==2-y ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][j][i][j] == mine ||
						  theboard->board[i][j][i][j] == TTT_NEUTRAL ||
						  theboard->board[i][j][i][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,4,5,4);
				}
				if ( (t == 1 || t == 2) && x==z && t==z && z==2-y ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][j][i][j] == mine ||
						  theboard->board[j][j][i][j] == TTT_NEUTRAL ||
						  theboard->board[j][j][i][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,4,5,5);
				}
			}
		}
		if ( y == 1 || y == 2 ) {
			// advance Y backwards
			if ( z == 0 || z == 1 ) {
				// advance Z
				if ( (t == 0 || t == 1) && x==y && t==z && y==2-z ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][i][j][j] == mine ||
						  theboard->board[i][i][j][j] == TTT_NEUTRAL ||
						  theboard->board[i][i][j][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,5,4,4);
				}
				if ( (t == 1 || t == 2) && x==t && t==y && y==2-z ) {
					// advance T backwards
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[j][i][j][j] == mine ||
						  theboard->board[j][i][j][j] == TTT_NEUTRAL ||
						  theboard->board[j][i][j][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,5,4,5);
				}
			}
			if ( z == 1 || z == 2 ) {
				// advance Z backwards
				if ( (t == 0 || t == 1) && x==y && x==z && x==2-t ) {
					// advance T
					gotya = 0;
					for ( i=0; i<3; ++i ) {
						j=2-i;
						if ( theboard->board[i][j][j][j] == mine ||
						  theboard->board[i][j][j][j] == TTT_NEUTRAL ||
						  theboard->board[i][j][j][j] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,5,5,4);
				}
				if ( (t == 1 || t == 2) && y==z && t==z && x==z ) {
					// advance T backwards
					gotya = 0;
					for ( i=2; i>=0; --i ) {
						if ( theboard->board[i][i][i][i] == mine ||
						  theboard->board[i][i][i][i] == TTT_NEUTRAL ||
						  theboard->board[i][i][i][i] == mineold ) {
							gotya++;
						}
					}
					if ( gotya == 3 ) SCORE(5,5,5,5);
				}
			}
		}
	}

	CHECKPOINT(15);
	return tempscore;
}

#define PDEFENSE 25
#define POFFENSE 25
#define DEFENSE  25
#define OFFENSE  25

void doAImove( char *x, char *y, char *z, char *t ) {
	struct coord4d bestspot = {4,4,4,4};
	char mine, notmine;
	int i,j,k,l, besttot = -1, temptot;

	showscore = 0;

	if ( whoseturn == 0 ) { mine = TTT_X_OWNS; notmine = TTT_O_OWNS; }
	  else { mine = TTT_O_OWNS; notmine = TTT_X_OWNS; }

	// Set up AI boards for calculations
	for ( i=0; i<3; ++i ) {
		for ( j=0; j<3; ++j ) {
			for ( k=0; k<3; ++k ) {
				for ( l=0; l<3; ++l ) {
					aiboard_offense.board[i][j][k][l] = boardstatus.board[i][j][k][l];
					if ( boardstatus.board[i][j][k][l] == TTT_UNOCCUPIED ) {
						// if the space is not occupied, pretend I
						// own it so I can judge how many potential
						// scores I could make if I owned everything
						aiboard_offense.board[i][j][k][l] = mine;

						// for defense calculations, pretend my
						// opponent owns all unoccupied squares and
						// see how many times he can score on a 
						// given position
						aiboard_defense.board[i][j][k][l] = notmine;
					}
				}
			}
		}
	}

	// Now calculate offense/defense values for each square, combine these values
	// linearly, and determine the best move from the results.

	for ( i=0; i<3; ++i ) {
		for ( j=0; j<3; ++j ) {
			for ( k=0; k<3; ++k ) {
				for ( l=0; l<3; ++l ) {
				  if ( boardstatus.board[i][j][k][l] == TTT_UNOCCUPIED ) {
					// if the REAL board has nothing here, calculate

					// Here's potential offense.  If the whole board
					// was mine...
					temptot = checkscore( l,k,j,i, &aiboard_offense )
					  * POFFENSE;

					// Here's immediate offense (using the real board)
					boardstatus.board[i][j][k][l] = mine;
					temptot += checkscore( l,k,j,i, &boardstatus )
					  * OFFENSE;
					boardstatus.board[i][j][k][l] = TTT_UNOCCUPIED;

					whoseturn = 1-whoseturn;
					// Step into my opponent's shoes...

					// Here's potential defense.  If the whole board
					// was my enemy's...
					temptot += checkscore( l,k,j,i, &aiboard_defense )
					  * PDEFENSE;

					boardstatus.board[i][j][k][l] = notmine;
					temptot += checkscore( l,k,j,i, &boardstatus )
					  * DEFENSE;
					boardstatus.board[i][j][k][l] = TTT_UNOCCUPIED;

					whoseturn = 1-whoseturn;
					// Back from out-of-body experience

					if ( temptot > besttot ) {
						// We found a better move.  Remember it.
						besttot = temptot;
						bestspot.x = l;
						bestspot.y = k;
						bestspot.z = j;
						bestspot.t = i;
					}
				  }
				}
			}
		}
	}

	*x = bestspot.x;
	*y = bestspot.y;
	*z = bestspot.z;
	*t = bestspot.t;

	showscore = 1;
}

int main(void)
{
	char selfplay = 0, player2 = 1;
	int or=0, og=0, ob=0;
	char myx, myy, myz, myt, gameover;
	char oldxx = 4, oldxy = 4, oldxz = 4, oldxt = 4;
	char oldox = 4, oldoy = 4, oldoz = 4, oldot = 4;
	int i,j,k,l,m, num_moves;

	msgbuf = malloc( 8192 );
	/* Start the MGL in the 640x480x256 display mode */
	dc = MGL_quickInit(640,480,8,1);

	system_font = MGL_loadFont( "pc8x8.fnt" );

	if ( MGL_result() == grFontNotFound ) {
		MGL_exit();
		printf( "\n\nUNABLE TO LOAD PC8x8.FNT!\n" );
		return 0;
	}

	MGL_useFont(system_font);

restartgame:
	xscore = oscore = 0;
	gameover = 0;
	MGL_setColorCI(MGL_BLACK);
	MGL_fillRectCoord( 0, 0, 639, 479 );

	for (i=0; i<3; ++i) for (j=0; j<3; ++j) for (k=0; k<3; ++k) for (l=0; l<3; ++l)
	  boardstatus.board[i][j][k][l] = TTT_UNOCCUPIED;

	mprintf( 0, 0, MGL_LIGHTRED, "4D TIC-TAC-TOE" );

	MGL_setColorCI(MGL_WHITE);

	for (i=0; i<3; ++i) {
		for (j=0; j<3; ++j) {
			for (k=2; k>=0; --k) {
				for (l=0; l<3; ++l) {
					for (m=0; m<4; ++m) {
						boardpoly[m].x = basepoly[m].x + 54+(l*36)-(k*20)+(j*214);
						boardpoly[m].y = basepoly[m].y + 36+(k*36)+(i*150);
					}
					boardpoly[4].x = boardpoly[0].x;
					boardpoly[4].y = boardpoly[0].y;
					MGL_polyLine( 5, boardpoly );
				}
			}
		}
	}

	if ( yesno( 0, 52, MGL_WHITE, "Should I create a neutral middle? (Y for YES)" ) ) {
		placemark( 1, 1, 1, 1, TTT_NEUTRAL );
	}

	blankline( 52 );

	if ( yesno( 0, 52, MGL_WHITE, "Would you like to watch me in self-play mode? (Y for YES)" ) ) {
		selfplay = 1;
		player2 = 0;
	} else {
		blankline(52);
		if ( yesno( 0, 52, MGL_WHITE, "Should player 2 be a computer AI player? (Y for YES)" ) ) {
			player2 = 0;
		}
	}

	blankline(52);

	MGL_getPaletteEntry( dc, MGL_LIGHTMAGENTA, (char*)&or, (char*)&og, (char*)&ob );
	MGL_setPaletteEntry( dc, TEMP_COLOR+1, or/2, og/2, ob/2 );
	MGL_setPaletteEntry( dc, TEMP_COLOR+2, or/2, og/2, ob/2 );
	MGL_realizePalette( dc, 2, TEMP_COLOR+1, 1 );

	num_moves = 0;

	while ( noquit ) {
		// main game loop
		if ( num_moves == 81 || 
		  (num_moves == 80 && boardstatus.board[1][1][1][1] == TTT_NEUTRAL) ) {
			gameover = 1;
			break;
		}
		blankline(2);
		mprintf( 0, 2, MGL_MAGENTA, "SCORING: " );
		mprintf( 10, 2, MGL_LIGHTMAGENTA, "X- " );
		mprintf( 13, 2, TEMP_COLOR+1, "%03d", xscore );
		mprintf( 20, 2, MGL_LIGHTMAGENTA, "O- " );
		mprintf( 23, 2, TEMP_COLOR+2, "%03d", oscore );
		for ( i=0; i<25; ++i ) {
			j = 25-i;
			if ( whoseturn == 0 ) {
				MGL_setPaletteEntry( dc, TEMP_COLOR+1, (or*(i+25))/50, (og*(i+25))/50, (ob*(i+25))/50 );
				MGL_setPaletteEntry( dc, TEMP_COLOR+2, (or*(j+25))/50, (og*(j+25))/50, (ob*(j+25))/50 );
			} else {
				MGL_setPaletteEntry( dc, TEMP_COLOR+2, (or*(i+25))/50, (og*(i+25))/50, (ob*(i+25))/50 );
				MGL_setPaletteEntry( dc, TEMP_COLOR+1, (or*(j+25))/50, (og*(j+25))/50, (ob*(j+25))/50 );
			}
			MGL_realizePalette( dc, 2, TEMP_COLOR+1, 1 );
		}
		blankline(51);
		mprintf( 0, 51, MGL_LIGHTGREEN, "%c's turn", (whoseturn ? 'O' : 'X') );
		if ( selfplay && num_moves == 0 ) {
			blankline(1);
			mprintf( 0, 1, MGL_LIGHTGREEN, "SELF-PLAY: Enter the first move for X and self-play will continue from there." );
		}
regetmove:
		if ( (!whoseturn && (!selfplay || num_moves == 0 )) ||
		  (whoseturn && player2) ) {
			if ( getcoords( &myx, &myy, &myz, &myt ) ) { noquit = 0; break; }
			CHECKPOINT(200);
			if ( boardstatus.board[myt][myz][myy][myx] != TTT_UNOCCUPIED ) {
				char *whosgotit;
				switch ( boardstatus.board[myt][myz][myy][myx] ) {
					case TTT_X_OWNS:
					case TTT_OLD_X_OWNS:
						whosgotit = "X";
					break;
					case TTT_O_OWNS:
					case TTT_OLD_O_OWNS:
						whosgotit = "O";
					break;
					default:
						whosgotit = "nobody";
				}
				blankline( 1 );
				mprintf( 0, 1, MGL_RED, "Position (%d,%d,%d,%d) is already occupied.  It is owned by %s.", myx+1, myy+1, myz+1, myt+1, whosgotit );
				goto regetmove;
			}
		} else {
			doAImove( &myx, &myy, &myz, &myt );
		}
		CHECKPOINT(201);
		blankline( 1 );
		showscore = 1;
		num_moves++;
		if ( !whoseturn ) {
			placemark( myx, myy, myz, myt, TTT_X_OWNS );
			if ( oldxx != 4 ) {
				placemark( oldxx, oldxy, oldxz, oldxt, TTT_OLD_X_OWNS );
			}
			oldxx = myx; oldxy = myy; oldxz = myz; oldxt = myt;
			xscore += checkscore( myx, myy, myz, myt, &boardstatus );
		} else {
			placemark( myx, myy, myz, myt, TTT_O_OWNS );
			if ( oldox != 4 ) {
				placemark( oldox, oldoy, oldoz, oldot, TTT_OLD_O_OWNS );
			}
			oldox = myx; oldoy = myy; oldoz = myz; oldot = myt;
			oscore += checkscore( myx, myy, myz, myt, &boardstatus );
		}
		whoseturn = 1 - whoseturn;
	}

	if ( gameover ) {
		blankline(2);
		mprintf( 0, 2, MGL_MAGENTA, "SCORING: " );
		mprintf( 10, 2, MGL_LIGHTMAGENTA, "X- " );
		mprintf( 13, 2, TEMP_COLOR+1, "%03d", xscore );
		mprintf( 20, 2, MGL_LIGHTMAGENTA, "O- " );
		mprintf( 23, 2, TEMP_COLOR+2, "%03d", oscore );

		if ( selfplay ) {
			if ( xscore > oscore ) mprintf( 0, 1, MGL_GREEN, "You picked a good starting position!  X won." );
			if ( oscore > xscore ) mprintf( 0, 1, MGL_GREEN, "You picked a crappy starting position!  O won." );
			if ( xscore == oscore ) mprintf( 0, 1, MGL_GREEN, "Stalemate... why there's a surprise." );
		} else {
			if ( !player2 ) {
				if ( oscore > xscore ) mprintf( 0, 1, MGL_GREEN, "I kicked your ass!  :-)" );
				if ( xscore > oscore ) mprintf( 0, 1, MGL_GREEN, "You kicked my ass!  :-(" );
				if ( oscore == xscore ) mprintf( 0, 1, MGL_GREEN, "Tie score!  Good game!" );
			} else {
				if ( xscore > oscore ) mprintf( 0, 1, MGL_GREEN, "Player 1 mopped the floor with Player 2." );
				if ( oscore > xscore ) mprintf( 0, 1, MGL_GREEN, "Player 2 utterly humiliated Player 1." );
				if ( xscore == oscore ) mprintf( 0, 1, MGL_GREEN, "Player 1 is about as good (or bad) as Player 2.  Tie score!" );
			}
		}
		blankline(52);
		if ( yesno( 0, 52, MGL_WHITE, "Would you like to play again?" ) ) {
			oldxx = 4; oldxy = 4; oldxz = 4; oldxt = 4;
			oldox = 4; oldoy = 4; oldoz = 4; oldot = 4;
			selfplay = 0;  player2 = 1;
			goto restartgame;
		}
	}

	MGL_unloadFont( system_font );
	/* Close down the MGL and exit */
	MGL_exit();
	free( msgbuf );
	return 0;
}
