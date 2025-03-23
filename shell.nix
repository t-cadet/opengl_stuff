{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "opengltuto";
  buildInputs = with pkgs; [
    clang_18
    glfw
    glew
    valgrind
  ];
}
