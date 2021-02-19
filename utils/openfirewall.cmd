@echo off
netsh advfirewall firewall add rule name="ATLAS" dir=in action=allow protocol=tcp localport=18000