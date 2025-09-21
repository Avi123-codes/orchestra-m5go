# Orchestra M5GO Display Animations

## Overview

The Orchestra M5GO features dynamic visual animations that enhance the musical experience. The display system provides both idle-time animations to keep the project engaging and synchronized playback animations that respond to the music.

## Animation States

### 1. Idle Animations (When Not Playing)

The system cycles through different idle animations every 15 seconds to keep the display interesting:

#### **Starfield Animation**
- 3D starfield effect with stars moving toward the viewer
- Stars get brighter as they approach
- Creates a sense of depth and motion
- Reminiscent of classic screensavers

#### **Wave Pattern**
- Multiple sine waves flowing across the screen
- Three layers of waves in different shades of blue
- Creates an ocean-like calming effect
- Waves move at different speeds for depth

#### **Rainbow Cycle**
- Smooth color gradient cycling through the entire spectrum
- Flows vertically down the screen
- Creates a vibrant, colorful display
- Speed-adjusted for smooth transitions

#### **Tinkercademy Logo**
- Displays the Tinkercademy branding
- Features the mascot character
- Animated glow effect around the logo
- Maintains project identity

### 2. Playback Animations (During Music)

Different animations are triggered based on the type of song being played:

#### **Solo Songs** (Purple LED)
The system cycles through four different visualizations:

1. **Equalizer Bars**
   - 20 vertical bars representing frequency bands
   - Heights respond to beat intensity
   - Color-coded: Green (low), Yellow (medium), Red (high peaks)
   - Classic music visualizer style

2. **Spiral Animation**
   - Colorful spiral emanating from center
   - Hue shifts create rainbow effect
   - Rotation speed synced to music tempo
   - Hypnotic, mesmerizing effect

3. **Particle System**
   - Particles burst from center of screen
   - Gravity affects particle movement
   - Each particle has unique color
   - Fading trails create firework-like effects

4. **Fireworks**
   - Random firework bursts across screen
   - Particles with physics simulation
   - Trailing effects for realism
   - Celebratory atmosphere

#### **Duet Songs** (Yellow LED)
- **Synchronized Circles**
  - Two circles orbiting the center
  - Represents the two parts playing together
  - Pulsing radius creates heartbeat effect
  - Circles maintain harmonic spacing

#### **Quintet Songs** (Green LED)
Alternates between two animations:

1. **Five Synchronized Circles**
   - Five circles representing all parts
   - Arranged in pentagonal formation
   - Each pulses at slightly different phase
   - Creates complex visual harmony

2. **Particle Burst**
   - More intense particle effects
   - Represents the full ensemble
   - Particles in quintet colors
   - Higher particle count than solo mode

## Visual Effects

### Color Coordination
- Animation colors match LED indicators:
  - Blue: Idle/Ready state
  - Purple: Solo performance
  - Yellow: Duet performance
  - Green: Quintet performance

### Particle Physics
- Gravity simulation for realistic motion
- Velocity-based trailing effects
- Life-based fading for smooth transitions
- Collision boundaries at screen edges

### Synchronization Features
- Frame-based animation timing
- Beat intensity parameter for music-reactive effects
- Device role awareness for customized animations
- Smooth transitions between animation states

## Technical Details

### Performance
- 30 FPS target framerate
- Double-buffered rendering (when fully implemented)
- DMA-optimized memory allocation
- Efficient pixel drawing algorithms

### Display Specifications
- Resolution: 320x240 pixels
- Color: 16-bit RGB565
- Controller: ILI9342C
- Interface: SPI

### Animation Techniques

#### HSV Color Space
- Used for smooth color transitions
- Enables rainbow effects
- Natural hue cycling

#### Mathematical Functions
- Sine waves for smooth oscillations
- Circular motion using trigonometry
- Pseudo-random generation for variety
- Physics simulation for particles

## Customization

### Per-Device Animations
Each device can have slightly different animation parameters based on its role:
- Conductor: Lead animation patterns
- Part 1-4: Offset phases for variety
- Creates cohesive but varied visual experience

### Future Enhancements

Potential additions to the animation system:

1. **Audio-Reactive Animations**
   - Real-time FFT analysis
   - Frequency-based visualizations
   - Beat detection for synchronized effects

2. **Network-Synchronized Effects**
   - Animations that span across all 5 screens
   - Coordinated color waves
   - Message-passing visual effects

3. **User Customization**
   - Selectable animation themes
   - Adjustable colors and speeds
   - Custom image uploads

4. **Advanced Effects**
   - 3D rendering capabilities
   - Shader-like effects
   - Video playback support

## Usage Tips

### Best Viewing Conditions
- Dim lighting enhances LED effects
- Arrange M5GOs in arc for best visual impact
- Keep screens at eye level

### Battery Optimization
- Animations automatically dim after extended idle
- Lower brightness options available
- Sleep mode after inactivity

### Troubleshooting

**No animations showing:**
- Check display initialization in serial output
- Verify SPI connections
- Ensure framebuffer allocation succeeded

**Choppy animations:**
- Reduce particle count
- Optimize animation complexity
- Check CPU usage

**Wrong colors:**
- Verify RGB565 conversion
- Check color byte order
- Ensure proper bit shifting

## Animation Flow

```
Power On
    ↓
Tinkercademy Logo (3 seconds)
    ↓
Idle Animation Cycle
    ├─ Starfield (15 sec)
    ├─ Wave Pattern (15 sec)
    ├─ Rainbow Cycle (15 sec)
    └─ Logo Display (15 sec)
    ↓
Button Press → Song Selected
    ↓
Playback Animation (based on song type)
    ├─ Solo: Equalizer → Spiral → Particles → Fireworks
    ├─ Duet: Synchronized Dual Circles
    └─ Quintet: Five Circles ↔ Particle Burst
    ↓
Song Ends
    ↓
Return to Idle Animation Cycle
```

This rich animation system ensures the Orchestra M5GO remains visually engaging whether actively playing music or sitting idle, creating an immersive audio-visual experience.