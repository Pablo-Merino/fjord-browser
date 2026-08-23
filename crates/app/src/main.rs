fn main() {
    let (major, minor, micro) = fjord_webkit::version();
    println!("Fjord plumbing ready (WPE WebKit {major}.{minor}.{micro})");
}
