cask "notepadnext" do
  version "0.14"
  sha256 "15608b53478bb46831a457e6f57397521fc8314ca39c8e2f1b309f95c96cc909"

  url "https://github.com/dail8859/NotepadNext/releases/download/v#{version}/NotepadNext-v#{version}.dmg"
  name "NotepadNext"
  desc "A cross-platform, reimplementation of Notepad++."
  homepage "https://github.com/dail8859/NotepadNext"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: :big_sur

  app "Notepadnext.app"
  binary "#{appdir}/Notepadnext.app/Contents/MacOS/NotepadNext", target: "notepad-next"

  uninstall quit: "io.github.dail8859.NotepadNext"

  zap trash: [
    "~/.config/NotepadNext",
    "~/Library/Application Support/NotepadNext",
    "~/Library/Saved Application State/io.github.dail8859.NotepadNext.savedState",
  ]
end
