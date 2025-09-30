# (C) Copyright 2016-2022 Xilinx, Inc.
# (C) Copyright 2023-2025 Advanced Micro Devices, Inc.
# Please add "source /path/to/bash-autocomplete.sh" to your .bashrc to use this.

_opt_filedir()
{
    # _filedir function provided by recent versions of bash-completion package is
    # better than "compgen -f" because the former honors spaces in pathnames while
    # the latter doesn't. So we use compgen only when _filedir is not provided.
    _filedir 2> /dev/null || COMPREPLY=( $( compgen -f ) )
}

_opt()
{
    local cur prev words cword arg flags w1 w2
    # If latest bash-completion is not supported just initialize COMPREPLY and
    # initialize variables by setting manualy.
    _init_completion -n 2> /dev/null
    if [[ "$?" != 0 ]]; then
        COMPREPLY=()
        cword=$COMP_CWORD
        cur="${COMP_WORDS[$cword]}"
    fi

    w1="${COMP_WORDS[$cword - 1]}"
    if [[ $cword > 1 ]]; then
        w2="${COMP_WORDS[$cword - 2]}"
    fi

    # bash always separates '=' as a token even if there's no space before/after '='.
    # On the other hand, '=' is just a regular character for clang options that
    # contain '='. For example, "-stdlib=" is defined as is, instead of "-stdlib" and "=".
    # So, we need to partially undo bash tokenization here for integrity.
    if [[ "$cur" == -* ]]; then
        # -foo<tab>
        arg="$arg$cur"
    elif [[ "$w1" == -*  && "$cur" == '=' ]]; then
        # -foo=<tab>
        arg="$arg$w1=,"
    elif [[ "$cur" == -*= ]]; then
        # -foo=<tab>
        arg="$arg$cur,"
    elif [[ "$w1" == -* ]]; then
        # -foo <tab> or -foo bar<tab>
        arg="$arg$w1,$cur"
    elif [[ "$w2" == -* && "$w1" == '=' ]]; then
        # -foo=bar<tab>
        arg="$arg$w2=,$cur"
    elif [[ ${cur: -1} != '=' && ${cur/=} != $cur ]]; then
        # -foo=bar<tab>
        arg="$arg${cur%=*}=,${cur#*=}"
    fi

    # Evaluate the path to the 'opt' command and get possible completions.
    eval local path=${COMP_WORDS[0]}
    flags=$( "$path" --autocomplete="$arg" 2>/dev/null | sed -e 's/\t.*//' | sed -e "s/^/${arg%%[^-]*}/" )
    # If 'opt' command does not support --autocomplete, fall back to file and directory completion.
    if [[ "$?" != 0 ]]; then
        _opt_filedir
        return
    fi

    # When opt does not emit any possible autocompletion, or user pushed tab after " ",
    # just autocomplete files.
    if [[ "$flags" == "$(echo -e '\n')" || "$arg" == "" ]]; then
        # If -foo=<tab> and there was no possible values, autocomplete files.
        [[ "$cur" == '=' || "$cur" == -*= ]] && cur=""
        _opt_filedir
    elif [[ "$cur" == '=' ]]; then
        COMPREPLY=( $( compgen -W "$flags" -- "") )
    else
        # Bash automatically appends a space after '=' by default.
        # Disable it so that it works nicely for options in the form of -foo=bar.
        [[ "${flags: -1}" == '=' ]] && compopt -o nospace 2> /dev/null
        COMPREPLY=( $( compgen -W "$flags" -- "$cur" ) )
    fi
}

# Register the _opt function as the completion handler for the 'opt' command.
complete -F _opt opt