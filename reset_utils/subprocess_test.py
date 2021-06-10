from subprocess import call
# reset USB
cmd = 'echo almexdi | sudo -S /home/smapa/Scanpy/reset_usb.sh'
call(cmd, shell=True)