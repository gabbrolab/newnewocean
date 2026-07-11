# Gabbro's Lab - Building an Ocean

## Production script

**Working title:** *I Started With a Sine Wave and Built an Ocean*  
**Target duration:** 28 to 32 minutes  
**Language:** English throughout: narration, on-screen labels, source callouts, and chapter cards.  
**Audience:** Science-curious viewers. No prior graphics or programming knowledge is assumed.  
**Narrative question:** *At what point does a wave become an ocean?*

This is not a C++ tutorial. Code appears only when it explains a visible change. The recurring distinction is:

- **Physics:** what the model is trying to capture.
- **Approximation:** what is simplified to run in real time.
- **Art direction:** what makes the image readable and cinematic.

The final FFT shot is not a traditional showcase opening. It appears for less than one second as a promise, then the film returns to the flat grid that begins the project.

---

## Capture list before editing

Record clean 16:9 clips for each point below. Leave two seconds of stillness before and after every action for editing room.

| Purpose | Checkout / command | Capture |
| --- | --- | --- |
| Empty beginning | `2c06a62 Add flat ocean grid` | Beauty view and `--wire` view of the flat grid. |
| First failure | `7c490a6 Add single sine wave mode` | A single sine wave from side and three-quarter angles. |
| Superposition | `07fd680 Layer multiple sine waves` | Single versus multi-sine match cut from the same camera. |
| Gerstner comparison | `68eb352` through `b67685f` | Side-on sine/Gerstner comparison, steepness changes, close crest. |
| Gerstner beauty | `43f92cc Make Gerstner crest foam visible` | Final Gerstner scene, plus close foam and reflection shots. |
| FFT spectrum | final branch | `--debug spectrum --cascade 0`. |
| FFT maps | final branch | `--debug height`, `--debug slope`, `--debug foam`, `--debug normals`. |
| Cascades | final branch | `--debug cascade --cascade 0`, then `1`, `2`, `3`, and `all`. |
| Final reward | final branch | Beauty captures from three slow camera paths, one wide, one surface-level, one close crest. |

Useful checkout pattern:

```powershell
git switch --detach <commit>
cmake --build build --config Debug
.\build\Debug\GabbroOcean.exe --capture captures\clip.bmp --frames 60 --width 1920 --height 1080
git switch codex/video-story-ocean
```

For recorded motion, run the application without `--capture` and use the same camera path for comparison shots. Do not show a terminal for more than a second unless it is part of a deliberately fast "commit history" montage.

---

## 0:00-0:35 - Cold open: The sine-wave promise

### On screen

Black background. Tablet writing appears in real time, but speed it up slightly so the writing feels intentional rather than slow:

```text
y(x, t) = A sin(kx - wt)
```

Cut to the clean single sine wave. Let it run for a beat. At the word **ocean**, flash the final FFT render for 8 to 12 frames: foam, sunset reflection, moving camera. Immediately hard cut back to the black tablet, then into the flat grid.

### Narration

> This is a sine wave.
>
> It is one of the simplest equations in physics.
>
> And somehow, with enough of them, it can become an ocean.
>
> Not a perfect ocean. Not a full fluid simulation. But an ocean that moves, reflects light, builds foam, and feels alive.
>
> The question is: at what point does a wave become an ocean?

### Edit notes

- Keep the final render flash unexplained. It is a question mark, not a reveal.
- No logo animation before this. A small `GABBRO'S LAB` mark can fade in after the line, not before it.
- Sound: one low hit on the final-render flash, then reduce the music to leave room for the grid.

---

## 0:35-1:40 - The blank grid

### On screen

Show the commit label in a restrained terminal-like overlay for one second:

```text
2c06a62  Add flat ocean grid
```

Show the wireframe ocean plane, then the shaded plane. Move the camera low across it so its total lack of motion is obvious. Briefly show the mesh-generation function and one vertex shader line, but do not scroll through code.

### Narration

> I am not starting with water. I am starting with a flat grid.
>
> This grid is the only piece of geometry in the whole project. It is just a lot of vertices arranged in a square.
>
> The important idea is that the grid is not the ocean yet. It is a surface waiting to be displaced.
>
> Every frame, a small program on the GPU - called a shader - decides where each of those vertices should move.
>
> That is the basic bargain behind this entire video. We do not sculpt every wave by hand. We write a rule, and let the same rule move thousands of points at once.

### Code shot

Highlight only the conceptual relationship:

```glsl
vec3 displaced = aPosition + displacement;
```

Say nothing about OpenGL setup, window creation, GLAD, or CMake here. The viewer only needs to understand that the grid is being moved.

### Transition

The wireframe gets a single vertical ripple. Freeze it just before the next section.

---

## 1:40-4:20 - What is a water wave?

### On screen / tablet

Draw a calm horizontal water line. Add a moving crest and trough. Label only as the words are spoken:

```text
crest       trough
amplitude A
wavelength lambda
period T
direction
```

Use one short, credited real-ocean clip or a still photograph here. The visual should be a broad wind-driven sea, not a dramatic breaking shore wave, because the project models deep-water surface waves rather than surf.

### Narration

> But before adding more waves, what is a water wave actually doing?
>
> The first surprising thing is that the water is not racing across the ocean with the crest.
>
> When wind blows across the surface, it transfers energy into the water. That energy travels. But a small parcel of water mostly moves in an orbit: up, forward, down, and back.
>
> The high point is the crest. The low point is the trough. The distance from one crest to the next is the wavelength. And the height from the calm surface to the crest is the amplitude.
>
> The period tells us how long one oscillation takes. The direction tells us where the wave pattern is travelling.
>
> This is already enough to make one useful model. It is not enough to make the sea.

### Source callout

Lower-right for three seconds:

```text
Ocean surface waves: NOAA Ocean Service
```

### Accuracy guardrail

Do not say that every water particle makes a perfect circle. Say "mostly moves in an orbit". The exact path depends on depth, wave shape, and non-linear effects.

---

## 4:20-6:40 - One sine wave

### On screen

Return to the tablet equation from the opening. Build it one variable at a time, then match each variable to a live viewport change. Use a fixed side camera for all parameter demonstrations.

```text
y(x, t) = A sin(kx - wt)
k = 2pi / lambda
```

Do not say "omega" only once and move on. Read it naturally: "omega" is the angular frequency, then state that it controls how fast the phase advances.

### Narration

> For one ideal wave, height is just amplitude times a sine.
>
> `A` controls how tall the wave is. Increase it, and the crests rise higher above the resting surface.
>
> The value `k` is related to wavelength. A larger `k` means the pattern repeats more often, so the waves are shorter.
>
> And `omega` controls how quickly the pattern moves through time.
>
> The important part is the phase: `kx minus omega t`. At a different position, or a different time, the sine gives us a different height.
>
> In code, that becomes a tiny rule: calculate a phase, take its sine, and move a vertex upward or downward.

### Code shot

Show a compact conceptual crop, not a full shader:

```glsl
float phase = dot(direction, world.xz) * frequency - time * speed;
world.y += amplitude * sin(phase);
```

### Narration, continued

> It is elegant. It is smooth. And it is completely unconvincing as an ocean.
>
> A single wave is too regular. Every crest is identical. Every trough is identical. It looks less like water, and more like a moving sheet.

### Live-coding option

Show a six-to-ten second speed-ramped live coding insert while changing amplitude or wavelength. Cut before compilation finishes, then reveal the visual result immediately. The point is causality, not typing skill.

---

## 6:40-9:00 - Many sine waves

### On screen

Use the same camera and split screen:

```text
ONE WAVE                         MANY WAVES
```

Layer waves one at a time. With each new wave, add a simple arrow showing its direction. Then hide the arrows once the surface is busy enough.

### Narration

> The first improvement is almost embarrassingly simple: add another wave.
>
> And another one.
>
> A real sea state is not one wave. It is a crowd of waves crossing, reinforcing, and cancelling each other.
>
> When two crests meet, the surface becomes higher. When a crest meets a trough, they partly cancel out. This is superposition.
>
> By giving the waves different amplitudes, wavelengths, speeds, and directions, the surface stops repeating so obviously.
>
> This is a surprisingly powerful trick. It already captures something real: ocean waves overlap. But the individual waves are still too smooth.
>
> The crests are rounded, the motion is mostly vertical, and the surface still feels like a collection of equations instead of a single body of water.

### Commit montage

```text
7c490a6  Add single sine wave mode
07fd680  Layer multiple sine waves
```

### Transition

Hold on a rounded crest. Draw a circle around it on the tablet. The circle turns into a sharper Gerstner crest in the next shot.

---

## 9:00-13:20 - Gerstner waves: shaping the crest

### On screen / tablet

Draw two cross-sections:

1. A sine wave, with arrows only up and down.
2. A Gerstner wave, with arrows that also pull horizontally toward the crest.

Then show a side-by-side program capture. Keep the same colour and camera position in both versions.

### Narration

> A sine wave only moves water up and down.
>
> Gerstner waves also pull it sideways.
>
> As a crest rises, nearby points are pulled toward it. That concentrates the surface near the top of the wave, creating a sharper crest and a broader trough.
>
> It is a small mathematical change, but visually it matters a lot. The wave finally starts to have a sense of mass and direction.
>
> We still control each wave with a direction, an amplitude, a wavelength, and a speed. But now we add steepness.
>
> More steepness makes the crest more dramatic. Too much steepness creates loops, where the mathematical surface folds over itself. That looks impressive for about half a second, and then it looks broken - because it is.

### Code shot

Show only the difference from the sine wave:

```glsl
world.xz += direction * steepness * amplitude * cos(phase);
world.y  += amplitude * sin(phase);
```

### Narration, continued

> This is also where the normal becomes important.
>
> A normal is simply the direction the surface is facing at a point. Light uses it to decide where to reflect, where to darken, and where to create a highlight.
>
> Instead of guessing the normal from neighbouring pixels, we can derive it from the Gerstner equations themselves. That keeps the lighting attached to the moving shape.

### Source callout

```text
Gerstner wave discussion: Mark Finch, GPU Gems, Chapter 1
```

### Commit sequence

```text
68eb352  Shape waves with Gerstner displacement
93a063a  Add multiple Gerstner waves
df9094f  Add analytical Gerstner normals
e48be66  Add fBM-like Gerstner wave layers
b67685f  Control Gerstner steepness
e9ea9d7  Add light domain warping
```

### Face-camera beat

After the loop warning, cut to face camera for one sentence:

> At this point, I could keep adding handcrafted waves forever. But that would not teach me how an ocean chooses its waves in the first place.

---

## 13:20-16:15 - Making it look like water

### On screen

Create rapid, labelled before/after transitions:

```text
SHAPE -> NORMALS -> LIGHT -> SKY REFLECTION -> FOAM
```

Use a beauty clip after each element. For foam, show a false-colour mask first, then dissolve into the final white foam. Do not claim the mask is a full model of breaking waves.

### Narration

> At this stage, the surface moves better. But movement alone is not water.
>
> Water becomes readable because of light. At shallow angles, it reflects the sky. On a tilted crest, the normal changes, so the reflection changes with it.
>
> I add a low sun, a sky environment, and a darker atmosphere toward the horizon. None of those things change the wave physics. They change how clearly we can see it.
>
> Foam is similar. In this project, foam is an artistic approximation guided by the steep and compressed parts of the surface. It is there to reveal energetic crests, not to claim that every bubble has been simulated.
>
> The wave shape is physically motivated. The colour, foam, and atmosphere are where simulation meets art.

### Code guidance

Show only one or two meaningful lines at a time:

```glsl
vec3 normal = normalize(vec3(-slopes.x, 1.0, -slopes.y));
vec3 envReflection = skyEnvironment(reflect(-viewDir, normal));
```

Do not explain Cook-Torrance, Beckmann, or every material uniform. They are implementation details, not the story of this video.

### Commit montage

```text
6b1fe8c  Add water lighting shader
9085000  Add skybox reflections
3fea014  Add crest foam
43f92cc  Make Gerstner crest foam visible
```

---

## 16:15-17:35 - Why Gerstner is still not the ocean

### On screen

Show the polished Gerstner beauty render. Then pause on a section where repeated directions or controlled wave patterns are visible. Overlay a few wave arrows to reveal the authored structure. Fade into the FFT spectrum debug view.

### Narration

> Gerstner waves are useful. They are fast, controllable, and they can look beautiful.
>
> But every wave is still one I chose by hand.
>
> I chose its length. I chose its direction. I chose how many waves existed.
>
> An ocean does not arrive as a short list of perfect waves. Wind puts energy into many different wavelengths and directions at the same time.
>
> So the next step is to stop drawing waves one by one.
>
> Instead, I want to describe the energy of the whole sea.

### Transition card

```text
FROM WAVES WE CHOOSE
TO WAVES THE WIND CAN EXCITE
```

---

## 17:35-21:20 - The ocean in frequency space

### On screen

Open directly on `--debug spectrum --cascade 0`. Let it look strange. Display a small caption:

```text
INITIAL WAVE SPECTRUM
```

Tablet drawing: left side is a water surface; right side is a grid with a bright directional region. Connect them with arrows labelled `FFT` and `inverse FFT`. Do not introduce complex-number notation before the image has landed.

### Narration

> Before it becomes blue water and foam, the ocean is a distribution of frequencies, slopes, and displacements.
>
> This image is not a height map. It is a spectrum.
>
> Instead of storing the height of every point on the ocean, a spectrum stores how much energy exists at different wavelengths and directions.
>
> Long waves live near one part of the spectrum. Short ripples live somewhere else. And wind gives more energy to waves travelling in its preferred direction.
>
> In the code, each frequency is described by a vector called `k`. Its length is related to wavelength, and its direction tells us which way that little wave component travels.
>
> To create the initial ocean, we generate random values, but we do not make them equally random. We shape them with a wave spectrum.
>
> Here I use a JONSWAP-style spectrum. It is based on measurements of wind-driven seas. Think of it as a statistical recipe: given wind conditions, which wave scales should be common, and which should be rare?
>
> That is why the final ocean can feel random without being arbitrary.

### Tablet formula

Write this slowly, then do not expand it further:

```text
H0(k) = random complex value * sqrt(S(k))
```

Say:

> `H zero of k` is the initial amplitude of one frequency component. `S of k` is the spectrum that decides how much energy that component is allowed to have.

### Source callout

```text
JONSWAP wave spectrum - Hasselmann et al., 1973
```

### Important wording

Say "JONSWAP-style" unless the exact implementation has been independently validated against the full published parameterisation. It is a physically motivated production model, not an ocean forecast.

---

## 21:20-24:20 - Letting every wave evolve

### On screen

Use a three-stage motion graphic made from program captures and tablet overlay:

```text
INITIAL SPECTRUM -> TIME UPDATE -> INVERSE FFT -> HEIGHT / DISPLACEMENT MAPS
```

Show the three matching commits as each stage is introduced. Put the final output of IFFT beside the abstract spectrum to make the transformation concrete.

### Narration

> Now the spectrum needs to move.
>
> Different wavelengths travel at different speeds. That is called dispersion.
>
> For deep-water gravity waves, the relationship is approximately:

```text
omega(k) = sqrt(g * |k|)
```

> Gravity is `g`. The length of `k` tells us the scale of the wave. Together, they tell us how quickly that frequency changes phase.
>
> Each little wave component evolves in time. Some move faster, some slower, and their phases drift apart.
>
> At this point, the data is still in frequency space. It is useful to the math, but it is not yet something I can put under a camera.
>
> The inverse fast Fourier transform - the IFFT - recombines all those frequency components into values in real space.
>
> In other words: it turns a recipe of waves into an actual surface.

### Implementation bridge

> This is why the FFT path runs on the GPU. Thousands of values have to be updated and transformed every frame. The GPU is already designed to perform the same kind of operation across a large grid of data.

### Code / pipeline shot

Show filenames rather than a dense compute shader:

```text
ocean_spectrum_update.comp
ocean_fft_horizontal.comp
ocean_fft_vertical.comp
ocean_assemble.comp
```

Then show the refactoring commits as readable milestones:

```text
3958172  Split FFT passes into spectrum update and IFFT
3eebcef  Assemble displacement slope and foam maps
```

### Source callout

```text
Spectral ocean simulation: Jerry Tessendorf, Simulating Ocean Water
```

---

## 24:20-27:20 - From data to ocean surface

### On screen

This is the most visual technical section. Use the actual debug modes, each introduced by a one-word card:

```powershell
--debug height
--debug slope
--debug foam
--debug normals
```

Then show the beauty render after each map is added. Avoid leaving a debug image static for more than five seconds; use it as a lens, then return to water.

### Narration

> The inverse transform gives us data. But data is not a finished ocean.
>
> First, height moves the vertices up and down. That gives the surface its swell.
>
> Then horizontal displacement shifts the surface sideways. This is the FFT version of the idea we saw with Gerstner waves: it makes the crests feel sharper and more directional. Artists often call that choppiness.
>
> Next come the slopes. A slope tells us how the height changes from one point to the next. From those slopes, the shader reconstructs a normal, and the normal tells light how to react.
>
> Finally, foam. The foam map accumulates activity around compressed, energetic parts of the surface and decays over time. It is not a full simulation of air, bubbles, and breaking water. It is a visual approximation with a physical hint behind it.
>
> The final image is not one texture. It is several physical clues combined in the shader.

### Debug-specific captions

| Debug view | On-screen caption | One sentence to say |
| --- | --- | --- |
| Height | `VERTICAL DISPLACEMENT` | "This is the part that lifts and lowers the grid." |
| Slope | `SURFACE ORIENTATION` | "This is what lets light know which way the water is facing." |
| Normals | `LIGHTING DATA` | "The colour is not water colour; it is the direction of the surface encoded as colour." |
| Foam | `CREST ACTIVITY` | "This highlights the places where the surface is most energetic in our approximation." |

### Commit references

```text
8bbb664  Apply FFT height displacement
a60d3f7  Add FFT slope normals
d031b20  Add choppy FFT displacement
ce41941  Add FFT foam detection
ac8eae8  Add foam accumulation
5f4f7ab  Debug FFT spectrum and cascade views
```

---

## 27:20-29:05 - Why multiple cascades matter

### On screen

Show four labelled captures with the same camera position:

```text
CASCADE 0 - LARGE SWELL
CASCADE 1 - MID-SCALE WAVES
CASCADE 2 - NEAR-SURFACE DETAIL
CASCADE 3 - FINE RIPPLE DETAIL
```

Use the real command form below in a small monospace overlay, then let the image do the work:

```powershell
--debug cascade --cascade 0
```

Repeat with `1`, `2`, `3`, then `all`.

### Narration

> There is one more problem. An ocean contains huge, slow swells and tiny, fast ripples at the same time.
>
> A single simulation grid cannot cover every scale well. If it covers a huge area, it loses fine detail. If it focuses on tiny detail, the large waves start repeating too quickly.
>
> The solution is to use several cascades.
>
> Each cascade covers a different range of wave scales. One gives the broad movement of the sea. Another adds medium waves. The smaller cascades add the detail that catches light near the camera.
>
> When they are combined, the ocean has structure at multiple scales - the thing a single procedural pattern usually fails to achieve.

### Commit references

```text
36b2025  Add multi-scale FFT cascades
916214a  Add FFT ocean quality presets
ebe2f5f  Rebuild FFT ocean as multi-cascade GPU simulation
```

---

## 29:05-31:00 - Final ocean and conclusion

### On screen

No debug overlays for the first 20 seconds. Let the final ocean breathe. Use the three planned camera paths: wide horizon, low glancing reflection, and close foam/crest detail. Then intercut one-second echoes of the flat grid, one sine wave, many sines, Gerstner, spectrum, and cascades.

### Narration

> This is where all those layers end up.
>
> A flat grid becomes a sine wave. Sine waves become a moving surface. Gerstner waves give that surface sharper crests. A spectrum lets wind distribute energy across many scales. The FFT turns that energy back into a surface every frame. And the shader turns displacement, slopes, foam, and light into something that reads as water.
>
> This is not a perfect ocean simulation.
>
> There is no shoreline. No boats disturbing the surface. No fully solved breaking waves. No Navier-Stokes fluid simulation underneath every pixel.
>
> But it is built from the same language as the real phenomenon: waves, energy, gravity, and light.
>
> And it all started with a sine wave.

### Final card

```text
GABBRO'S LAB
PHYSICS, MADE VISIBLE
```

Keep this card short. End on the moving ocean, not a static subscribe screen. Any subscribe prompt belongs as a quiet verbal line over the last five seconds, or in the end screen after the final visual sentence.

---

## Spoken source mentions

Use sources as part of the investigation, not as a citation after every formula.

| Moment | Optional spoken line | On-screen text |
| --- | --- | --- |
| Deep-water wave context | "For the basic physics of wind-driven surface waves, I started with NOAA's explanation of how wind transfers energy into the sea." | `NOAA Ocean Service` |
| Gerstner | "A really useful graphics reference here is Mark Finch's GPU Gems chapter, which starts from sine waves and explains why Gerstner waves create sharper crests." | `Finch, GPU Gems` |
| Spectrum | "For the FFT approach, I went back to Jerry Tessendorf's ocean-simulation notes - one of the references that made this technique so widely used in graphics." | `Tessendorf, Simulating Ocean Water` |
| JONSWAP | "The energy distribution is inspired by JONSWAP, a spectrum developed from measurements of wind-driven seas." | `Hasselmann et al., 1973` |

Put complete links in the video description:

1. NOAA Ocean Service, [Why does the ocean have waves?](https://oceanservice.noaa.gov/facts/wavesinocean.html)
2. Mark Finch, [Effective Water Simulation from Physical Models](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models)
3. Jerry Tessendorf, [Simulating Ocean Water](https://jtessen.people.clemson.edu/reports/papers_files/coursenotes2004.pdf)
4. K. Hasselmann et al., [JONSWAP report](https://pure.mpg.de/rest/items/item_3262854_4/component/file_3262856/content)

---

## Editing rules

- The viewer should see the result of an equation within ten seconds of its introduction.
- Keep source callouts small and legible; do not turn the video into a bibliography.
- Never call the FFT result "random" without adding that its randomness is shaped by a physical spectrum.
- Never imply the foam map is a fully resolved breaking-wave simulation.
- When code is shown, highlight one idea and keep the crop under eight lines.
- Prefer before/after comparisons from the same camera over generic beauty shots.
- Use commit identifiers sparingly: they establish the project history, but should not interrupt the narration.
- The final FFT render is the reward for understanding the journey, not the opening subject of the video.
