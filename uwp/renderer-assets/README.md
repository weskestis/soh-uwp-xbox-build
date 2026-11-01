# Fusion OpenGL runtime resources

The pinned OoT and MM cores request `shaders/opengl/default.shader.glsl` for both
vertex and fragment stages. The retained OoT port archive contains no shaders;
MM contains older `.fs`/`.vs` files that do not satisfy this lookup. OoT 1.0.5.1
therefore reached its launcher, then called `abort()` on the first game shader.

`tools/fusion_package_renderer.py` appends this raw text resource and its MIT
notices to each verified port archive, preserving every original member byte.
There is no legacy binary resource header: the cores provide explicit Shader
resource metadata and the factory reads text from byte zero through EOF.

Source: [Kenix3/libultraship, commit 7cfca877e64d70e19552eb47ab2b3540f53bbe9a](https://github.com/Kenix3/libultraship/blob/7cfca877e64d70e19552eb47ab2b3540f53bbe9a/src/fast/shaders/opengl/default.shader.glsl).
Original Git blob: `1fe86aa88ef259ff231e98358befd3610d2aec7e` (9,647 bytes).
Only CRLF line endings are normalized to LF. The SHA-256 of the distributed text
is `6cf7e3ccb94b41f85c974b0caec38350ea10cc4667e367c95919072075b4ac5e`.
This version retains the `mod` combiner-wrap behavior used by this codebase;
later upstream changed that expression. The template has no include dependencies.

Assembly records both base and resulting archive hashes plus renderer provenance.
The signed and unsigned package audits verify the shader path and exact bytes in
both archives, so an otherwise valid installer cannot omit this resource again.
