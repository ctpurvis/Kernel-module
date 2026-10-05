
set -e

TARGET=/tmp/test.txt

echo "== Writing test file with 'bat' on disk =="
echo "I saw a bat fly past the bat cave." > "$TARGET"
echo "--- on-disk content (before load) ---"
cat "$TARGET"

echo
echo "== Loading module (default: watches $TARGET, bat -> cat) =="
sudo insmod Kernel_module.ko

echo
echo "== Reading the file while the module is loaded =="
echo "--- what a reader sees (should say 'cat') ---"
cat "$TARGET"

echo
echo "== Unloading module =="
sudo rmmod Kernel_module

echo
echo "== Reading the file again (module unloaded) =="
echo "--- on-disk content (should still say 'bat') ---"
cat "$TARGET"

echo
echo "== dmesg tail =="
dmesg | tail -n 15
