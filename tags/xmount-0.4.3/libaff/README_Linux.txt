INSTALLING ON Ubuntu/Kbuntu/Debian LINUX

(I'm sorry that this is so complicated.)

1. Edit /etc/apt/sources and uncomment the lines with "universe"
2. apt-get update

General build tools:
3. apt-get -y install make gcc g++ 

Libraries required for AFFLIB:
4. apt-get -y install zlib1g-dev libssl-dev libncurses5-dev 
5. apt-get -y install libcurl3-dev libexpat1-dev libreadline5-dev

Libraries if you want to make a release:
6. apt-get -y install automake1.9  autoconf libtool

If you want a generally usable system (e.g., for EC2)
7. apt-get -y install dbootstrap openssh-client openssh-server subversion emacs


================================================================
INSTALLOING ON FEDORA CORE 6:

1. When you build Linux, tell it that you want developer tools.

   yum upgrade all
   yum install libssl-dev libncurses5-dev 
