wc -l `find ./ \( -name "*.m" -or -name "*.c" -or -name "*.h" \) -and -not \( -path "*Historical*" -or -path "*Gource*" \)`
