# Boron
<img width="60" height="60" alt="image" src="https://github.com/user-attachments/assets/276c5e4a-2821-4461-9245-51984a8e255d" />

A modifiyed/forked Erbium gameserver with my own features and fixes

### info and updates are coming soon.. :)


### Todo:

<details>
  

- [ ] **LateGame**
  - [ ] Realisic mooving bus  *(config + GUI toggle added, soon needs some more work)*
  - [ ] Custom lootpools.
  - [ ] Zone damage on older builds.
  
- [ ] **Creative**
  - [ ] Give "extra" ammo from menu.  *(config flag added, currently bugs out inventory)*
  
- [ ] **GUI**
  - [ ] Player manager tab. (soon)
  - [x] Calendar tab. (e.g: full snow map on s11)  *(s11 snow wired; s13 water + s19 snow still needs so work tho)*
  - [x] LateGame tab. *(slots 1-5 + counts + moving-bus toggle should work)*
  - [x] Pre-StartServer options (Main tab's new feature). (for manual server setup mode)  *(MANUAL_SERVER_SETUP + "Setup server" button done)*
  
- [ ] **Gameplay**
  - [ ] Projectile guns on 28.xx and 29.xx versions. **5% Done** *(scaffold in Misc.cpp, hook disabled at :658)*
  - [x] Fix 16.xx crashes. (ServerHandlePickup was the issue nwo its fixed)
  - [ ] Some LTMs stuff.
  - [ ] Placement points for Arena.
  - [ ] Bosses on 12.xx, 13.xx, 14.xx **20% Done** ( this is buggy rn and laggy a bit but tested on 13.40 a few)
  - [ ] Fix autorestart (maybe in 6-7 weekls)

- [ ] **Commands**
  - [x] Server message. **100% done** (should work well on 1.8 - 21.00)
  - [ ] Faster item give.  *(ItemAliases.h resolver added, needs some command wiring tho)*
  - [ ] POI tp commands (hopefully on every season, later)

- [ ] **Versions** (not decided yet)
  - [ ] ~~ 30.20  *(version detection already handles >=30)* ~~
  - [x] 31.41 *(maybe but it should be less fcked)* **9% Done**


</details>

### S30+ EXPERMENTAL Support info.

<details>

Currently the only version that is "supported" is **31.41** C5S4.

Progress: **9% Done**

**Features (currently a lot missing)**
- Walking simulator
- In-game (BR island)
- MME stuff (hurdle still needs fix and sprint needs stamin fix)
- Chest, Ammo box looting
- No pickaxe (yet)
- No proper NetMode (yet)
- No building and editing (yet)


</details>


If any1 has any suggestions what commands, fixes, gui additions i should add let me know.

---


<details>
  
<summary>Erbium</summary>

Erbium is a WIP universal gameserver for Fortnite.

[**Erbium discord**](https://discord.gg/WxNEGBxfKq)

## Features
- **Version support**: Erbium supports version 3.4 (season 3) to 19.40 (chapter 3 season 1).
> Erbium also has partial support for 1.7.2 (season 0) to 3.3 (season 3) & 20.00 (chapter 3 season 2) to 30.00 (chapter 5 season 3)
- **Easy to use**: Erbium is designed to be fast & easy to configure for new users.

## To-do
- **Creative**
- **XP**
- **Quests**

## Configuration
- In order to change options, go to Erbium/Public/Configuration.h & configure to your liking!

> If you have issues with a specific version or find a bug, please make an issue.

> Contributions are highly appreciated!
</details>


> ## Credits
> If you use this, please give credits to: [Ploosh](https://github.com/plooshi).
> 
> Credit to Milxnor for parts of Finders.cpp
