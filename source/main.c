#include <stdio.h>
#include <switch.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    consoleInit(NULL);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    printf("Random NRO Ideas\n");
    printf("-----------------\n\n");
    printf("Starter homebrew is running.\n\n");
    printf("Press + to return to Homebrew Menu.\n");

    while (appletMainLoop())
    {
        padUpdate(&pad);

        const u64 buttons_down = padGetButtonsDown(&pad);
        if (buttons_down & HidNpadButton_Plus)
            break;

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
