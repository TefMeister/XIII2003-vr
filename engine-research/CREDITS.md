# Credits & Attribution

This project is a reverse-engineering and modding effort built on the public
research, tools, and creative work of many people who came before us. None of
this would be possible without them. We list every source, tool, and prior
work we have drawn on below — by name or handle, as accurately as we could
verify it — including those that helped only as inspiration.

If we have missed someone, the omission is a mistake, not a slight. Please see
the "Get credited, or ask us to stop" section at the bottom.

## The original game

XIII (2003) is the creative work of its developer and publisher. We are only
modding it; we did not make it, and all rights to the game and its assets
belong to their owners. No game files are included in any repository in this
project.

| Work | Creator(s) | Note |
|---|---|---|
| XIII (2003), original "Classic" game | Ubi Soft Paris (developer); Ubi Soft (publisher) | Cel-shaded FPS adapting the Belgian comic *XIII* by Jean Van Hamme & William Vance. |
| Unreal Engine 2.x (the base engine) | Epic Games | The engine XIII is built on; source of the RenderDevice plugin model and the `FRotator` convention. |

## Tools, frameworks, and prior research this project builds on

| Source / Work | Creator(s) | Link |
|---|---|---|
| MinHook (function-hooking library) | Tsuda Kageyu (TsudaKageyu) and contributors | https://github.com/TsudaKageyu/minhook |
| OpenVR / SteamVR (VR runtime and compositor target; the hardware-verified overlay path) | Valve | https://github.com/ValveSoftware/openvr |
| OpenXR (cross-vendor VR runtime standard) | The Khronos Group and contributors | https://www.khronos.org/openxr/ |
| Virtual Desktop / VDXR (Quest-to-PC VR streaming and OpenXR runtime) | Guy Godin / Virtual Desktop, Inc. | https://www.vrdesktop.net |
| doctest (unit-testing framework) | Viktor Kirilov (onqtam) and contributors | https://github.com/doctest/doctest |
| x64dbg (debugger) | mrexodia, Sigma, tr4ceflow, Dreg, Nukem, Herz3h, torusrxxx, and the x64dbg contributor community | https://github.com/x64dbg/x64dbg |
| x64dbg-automate (remote-automation plugin + Python client) | dariushoule (Darius Houle) | https://github.com/dariushoule/x64dbg-automate |
| x64dbg-skills (reverse-engineering skill guides) | dariushoule (Darius Houle) | https://github.com/dariushoule/x64dbg-skills |
| Superpowers (skills framework used during development) | Jesse Vincent (GitHub: obra) and contributors at Prime Radiant | https://github.com/obra/superpowers |
| Legacy-framebuffer-to-spatial-VR and native-ABI VR strategy guides (the two staged milestones this project follows) | Brobert-in-aus (`guides` repo) | https://github.com/Brobert-in-aus/guides |
| AI development assistance | Claude (Anthropic) | https://www.anthropic.com |

Project lead and author: **TefMeister**.

Where a handle or attribution above is uncertain, we have said so, or we have
linked the source so anyone can check it. If you can correct or confirm a
detail, please open a GitHub issue — we would much rather fix it than leave it
wrong.

## Get credited, or ask us to stop

**If you helped and are not credited:** if you contributed anything to this
work — code, research, tools, documentation, or even just an idea that inspired
a part of it — and you do not see yourself credited above, that is an oversight
on our part, not a judgement about your contribution. Please contact us by
opening a GitHub issue on this repository, and we will correct the credits as
soon as possible.

**If you want your work removed or not used:** if you are the owner or creator
of something referenced or used here, and you would rather your work not be
referenced in this project, or you want specific content removed, please tell
us by opening a GitHub issue. We will honour that request promptly — no
argument and no delay — and we will find another way to do the job that does
not rely on your material. This is your work; we are only grateful to have
learned from it.
