#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

  system("rmmod ehci_hcd uhci_hcd");
	system("echo mem > /sys/power/state");
  //system("modprobe uhci_hcd ehci_hcd");
	printf("echo end\n");

  return 0;
}
