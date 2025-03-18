{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    curl
    jsoncpp
    cmake
  ];

  # This will set up promopt
  shellHook = ''
    if [ -f ~/.bash_profile ]; then
        source ~/.bash_profile
    fi
    echo "Nix-shell started..."
    export PS1="\[\033[36m\]\u\[\033[m\]@\[\033[32m\]nix:\[\033[33;1m\]\w\[\033[m\]:\[\033[31;1m\]\$(parse_git_branch)\[\033[m\]\$ "
  '';
}
