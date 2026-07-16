class Basica < Formula
  desc "Small IBM BASICA-compatible interpreter clone written in C"
  homepage "https://github.com/kdekorte/basica"
  url "https://github.com/kdekorte/basica/archive/refs/tags/v0.99.1.tar.gz"
  # sha256 "REPLACE_WITH_ACTUAL_SHA256_AFTER_TAGGING_RELEASE"
  license "MIT"

  depends_on "pkg-config" => :build
  depends_on "sdl3"
  depends_on "sdl3_image"
  depends_on "sdl3_mixer"
  depends_on "sdl3_ttf"

  def install
    system "make", "CC=#{ENV.cc}", "PREFIX=#{prefix}"
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    assert_match "BASICA", shell_output("#{bin}/basica --version")
  end
end
