#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc,char **argv)
{
    printf("Making sure all scripts are executable\n");
    system("chmod +x *.sh");
    exit(0);
}
