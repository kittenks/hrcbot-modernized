// Button.h
// User-command button bit flags.  These are the stable engine values defined
// by the game's in_buttons.h; they are reproduced here so the plugin does not
// have to pull game/server shared headers into its include path.
#ifndef HRCBOT_CORE_BUTTON_H_
#define HRCBOT_CORE_BUTTON_H_

#define IN_ATTACK        (1 << 0)
#define IN_JUMP          (1 << 1)
#define IN_DUCK          (1 << 2)
#define IN_FORWARD       (1 << 3)
#define IN_BACK          (1 << 4)
#define IN_USE           (1 << 5)
#define IN_CANCEL        (1 << 6)
#define IN_LEFT          (1 << 7)
#define IN_RIGHT         (1 << 8)
#define IN_MOVELEFT      (1 << 9)
#define IN_MOVERIGHT     (1 << 10)
#define IN_ATTACK2       (1 << 11)
#define IN_RUN           (1 << 12)
#define IN_RELOAD        (1 << 13)
#define IN_ALT1          (1 << 15)
#define IN_ALT2          (1 << 16)
#define IN_SCORE         (1 << 17)
#define IN_SPEED         (1 << 12)
#define IN_WALK          (1 << 13)
#define IN_ZOOM          (1 << 16)
#define IN_WEAPON1       (1 << 15)
#define IN_WEAPON2       (1 << 16)
#define IN_BULLRUSH      (1 << 22)

#endif // HRCBOT_CORE_BUTTON_H_
