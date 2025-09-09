#!/bin/zsh
# --- info ---
version="$(brl version | sed -e 's/Bedrock Linux //')"
kernel="$(uname -sr | sed -e 's/Linux //')"
uptime="$(uptime -p | sed 's/up //')"
mem="$(free -h | awk '/Mem:/ {print $3 " / " $2}')"

# --- get WM ---
if [ -n "$XDG_CURRENT_DESKTOP" ]; then
    wm="$XDG_CURRENT_DESKTOP"
elif [ -n "$DESKTOP_SESSION" ]; then
    wm="$DESKTOP_SESSION"
elif pgrep -x "i3" > /dev/null; then
    wm="i3"
elif pgrep -x "sway" > /dev/null; then
    wm="Sway"
elif pgrep -x "bspwm" > /dev/null; then
    wm="bspwm"
elif pgrep -x "dwm" > /dev/null; then
    wm="dwm"
elif pgrep -x "awesome" > /dev/null; then
    wm="Awesome"
else
    wm="Unknown"
fi

# --- get terminal ---
terminal="$(ps -p $(ps -p $$ -o ppid=) -o comm= 2>/dev/null || echo "Unknown")"

# --- get shell ---
shell="$(basename "$SHELL")"

# --- get CPU ---
cpu="$(lscpu | grep 'Model name' | cut -d':' -f2 | sed 's/^[ \t]*//' | head -n1)"
if [ -z "$cpu" ]; then
    cpu="$(grep 'model name' /proc/cpuinfo | head -n1 | cut -d':' -f2 | sed 's/^[ \t]*//')"
fi

# --- get GPU ---
gpu="$(lspci | grep -E "VGA|3D" | cut -d':' -f3 | sed 's/^[ \t]*//' | head -n1)"
if [ -z "$gpu" ]; then
    gpu="Unknown"
fi

# --- package counts per strata ---
pkgcounts=()
for strata in $(brl list); do
    case "$strata" in
        fedora)
            count="$(rpm -qa | wc -l)"
            pkgcounts+=("$count (rpm)")
            ;;
        gentoo)
            count="$(qlist -I | wc -l 2>/dev/null || equery list '*' | wc -l)"
            pkgcounts+=("$count (emerge)")
            ;;
        tut-arch)
            count="$(pacman -Qq | wc -l)"
            pkgcounts+=("$count (pacman)")
            ;;
        bedrock)
            count="$(nix profile list | tail -n +2 | wc -l 2>/dev/null)"
            pkgcounts+=("$count (nix)")
            ;;
    esac
done
packages="$(IFS=', '; echo "${pkgcounts[*]}")"

# --- Nord color palette ---
bold="$(tput bold)"
reset="$(tput sgr0)"

# Nord Polar Night
nord0="$(tput setaf 0)"    # #2E3440
nord1="$(tput setaf 8)"    # #3B4252
nord2="$(tput setaf 7)"    # #434C5E
nord3="$(tput setaf 15)"   # #4C566A

# Nord Snow Storm
nord4="$(tput setaf 15)"   # #D8DEE9
nord5="$(tput setaf 7)"    # #E5E9F0
nord6="$(tput setaf 15)"   # #ECEFF4

# Nord Frost
nord7="$(tput setaf 6)"    # #8FBCBB
nord8="$(tput setaf 14)"   # #88C0D0
nord9="$(tput setaf 4)"    # #81A1C1
nord10="$(tput setaf 12)"  # #5E81AC

# Nord Aurora
nord11="$(tput setaf 1)"   # #BF616A
nord12="$(tput setaf 9)"   # #D08770
nord13="$(tput setaf 3)"   # #EBCB8B
nord14="$(tput setaf 2)"   # #A3BE8C
nord15="$(tput setaf 5)"   # #B48EAD

# --- print it out! ---
echo ${reset}${bold}" ┌──┐"${nord1}${bold}" ┌──────────────────────────────────┐ "${nord11}${bold}"┌────┐"
echo ${reset}${bold}" │"${nord1}▒▒${reset}${bold}"│"${nord1}${bold}" │─"${reset}${bold}"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"${nord1}${bold}"─────────────────────│ "${nord11}${bold}"│ 境 │"
echo ${reset}${bold}" │"${nord0}██${reset}${bold}"│"${nord1}${bold}" │──"${reset}${bold}"\\\\\\\\\\\\      \\\\\\\\\\\\"${nord1}${bold}"────────────────────│ "${nord11}${bold}"│    │"
echo ${reset}${bold}" │"${nord1}██${reset}${bold}"│"${nord1}${bold}" │───"${reset}${bold}"\\\\\\\\\\\\      \\\\\\\\\\\\"${nord1}${bold}"───────────────────│ "${nord11}${bold}"│ 界 │"
echo ${reset}${bold}" │"${nord11}██${reset}${bold}"│"${nord1}${bold}" │────"${reset}${bold}"\\\\\\\\\\\\      \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"${nord1}${bold}"────│ "${nord11}${bold}"└────┘"
echo ${reset}${bold}" │"${nord12}██${reset}${bold}"│"${nord1}${bold}" │─────"${reset}${bold}"\\\\\\\\\\\\                    \\\\\\\\\\\\"${nord1}${bold}"───│"
echo ${reset}${bold}" │"${nord13}██${reset}${bold}"│"${nord1}${bold}" │──────"${reset}${bold}"\\\\\\\\\\\\                    \\\\\\\\\\\\"${nord1}${bold}"──│"
echo ${reset}${bold}" │"${nord14}██${reset}${bold}"│"${nord1}${bold}" │───────"${reset}${bold}"\\\\\\\\\\\\        ──────      \\\\\\\\\\\\"${nord1}${bold}"─│"
echo ${reset}${bold}" │"${nord7}██${reset}${bold}"│"${nord1}${bold}" │────────"${reset}${bold}"\\\\\\\\\\\\                   ///"${nord1}${bold}"─│"
echo ${reset}${bold}" │"${nord8}██${reset}${bold}"│"${nord1}${bold}" │─────────"${reset}${bold}"\\\\\\\\\\\\                 ///"${nord1}${bold}"──│"
echo ${reset}${bold}" │"${nord9}██${reset}${bold}"│"${nord1}${bold}" │──────────"${reset}${bold}"\\\\\\\\\\\\               ///"${nord1}${bold}"───│"
echo ${reset}${bold}" │"${nord10}██${reset}${bold}"│"${nord1}${bold}" │───────────"${reset}${bold}"\\\\\\\\\\\\////////////////"${nord1}${bold}"────│"
echo ${reset}${bold}" │"${nord15}██${reset}${bold}"│"${nord1}${bold}" └──────────────────────────────────┘"
echo ${reset}${bold}" │"${nord7}██${reset}${bold}"│ "${nord12}"Version: "${nord4}${version}
echo ${reset}${bold}" │"${nord8}██${reset}${bold}"│ "${nord12}"Kernel: "${nord4}${kernel}
echo ${reset}${bold}" │"${nord9}██${reset}${bold}"│ "${nord15}"Uptime: "${nord4}${uptime}
echo ${reset}${bold}" │"${nord10}██${reset}${bold}"│ "${nord15}"WM: "${nord4}${wm}
echo ${reset}${bold}" │"${nord15}██${reset}${bold}"│ "${nord15}"Packages: "${nord4}${packages}
echo ${reset}${bold}" │"${nord11}██${reset}${bold}"│ "${nord13}"Terminal: "${nord4}${terminal}
echo ${reset}${bold}" │"${nord12}██${reset}${bold}"│ "${nord13}"Memory: "${nord4}${mem}
echo ${reset}${bold}" │"${nord13}██${reset}${bold}"│ "${nord13}"Shell: "${nord4}${shell}
echo ${reset}${bold}" │"${nord14}██${reset}${bold}"│ "${nord9}"CPU: "${nord4}${cpu}
echo ${reset}${bold}" │"${nord1}▒▒${reset}${bold}"│ "${nord9}"GPU: "${nord4}${gpu}
echo ${reset}${bold}" └──┘"${reset}