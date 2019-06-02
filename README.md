# PortableWebsite

This project describes the use of a M32C/84 as an embedded internet appliance, capable of hosting a website over a standard dialup connection. 

When this device connects itself to the internet, it announces its allocated IP address to a dynamic DNS service, 
allowing a fixed domain name to point directly to HTTP and FTP servers. 
This provides a simple, low-power web-hosting option for web sites. 

The data served by the website is stored on a low-cost compact flash card, 
supporting complex websites which can be updated easily using the inbuilt FTP server, 
or externally using a PC with a compact flash reader. The M32C/84 approach used a minimum number of ICs, 
and allowed a relatively fast memory-mapped interface to the compact flash card.
