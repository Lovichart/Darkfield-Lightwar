# SFML vs raw OpenGL 3.3：暗域光战实体渲染层评估

日期：2026-10-28  
实验目录：`H:\C\Darkfield Lightwar\worktrees\gl\`（主工作区 `src/`、`include/` 未被修改）  
结论先行：**不换。** 1k–10k 艘船的实体渲染不是当前瓶颈；raw GL instancing 在独显上只省约 **1.6 ms/帧**（10k 时实体 pass 3.4→1.8 ms），在游戏实际使用的核显上 **没有收益甚至略慢**。先做「三角函数外提」这类零风险改动即可。

---

## 1. 测试方法

独立 benchmark（`bench/src/bench.cpp`，不链接游戏代码），用与 `Renderer::drawShips()` **逐字相同**的数学建船体三角形 + 1.55× 加色光晕，两个批次（体/光晕各一次 draw，混合模式与游戏一致）：

| 后端 | 做法 |
|---|---|
| A. SFML | 两个 `sf::VertexArray`（Triangles），每帧重建后 `window.draw()`；与当前 Renderer 一致 |
| B. raw GL 3.3 instanced | 1 个 VBO（每实例 pos/rot/scale/color，32 B），`glDrawArraysInstanced`，顶点在 shader 里用 `gl_VertexID` 展开；2 次 draw（体 + 光晕，混合不同） |
| C. raw GL 3.3 stream | 与 A 完全相同的 CPU 逐顶点数据（20 B/顶点，含 uv），`BufferData(NULL)+BufferSubData` 后 `glDrawArrays`；2 次 draw |

测量：
- 每个配置 **160–200 帧，丢弃前 20 帧**；每帧 `clear → build → submit → glFinish → display`，vsync 关闭。
- 预热 1–2 s（抬 GPU 时钟；实测独显跑到 1785 MHz）。
- `build` = CPU 几何生成；`submit` = draw 调用本身；`gpu` = `GL_TIME_ELAPSED` timer query（纯 GPU 光栅化时间）；`frame` = 到 `glFinish` 为止的整帧；`swap` 另计。
- 窗口 1600×900、4× MSAA、隐藏（不抢焦点、不干扰游戏窗口），与 `Game::Game()` 的窗口设置一致。
- 同一套代码编译两个 exe：`bench.exe`（默认 Optimus，游戏实际走核显）与 `bench_nv.exe`（导出 `NvOptimusEnablement`，强制 GTX 1650）。

硬件/上下文：
- i5-10300H（4C/8T）、16 GB、Windows 10 19045、高性能电源、插电。
- **Intel UHD Graphics**（1920×1080@60 主屏，游戏默认 GPU），GL 4.6 兼容上下文。
- **NVIDIA GTX 1650 4 GB**（Optimus，驱动 581.83），GL 4.6。
- SFML 2.6.2 + VS2022 v143 /O2 /std:c++17。
- 空场景底噪（clear+present+glFinish）：Intel ≈ 1.0–1.5 ms，NVIDIA ≈ 2.5–2.8 ms（Optimus 隐藏窗口的呈现开销，已单列）。

---

## 2. 数字

### 2.1 Intel UHD（游戏实际默认 GPU）——整帧 avg ms

| 船数 | SFML 逐顶点（当前代码） | SFML 外提 sin/cos | GL 3.3 instanced | GL 3.3 stream |
|---:|---:|---:|---:|---:|
| 1 000 | 1.84 | 1.77 | 1.64 | 1.82 |
| 5 000 | **3.40** | 3.62 | 4.42 | 4.55 |
| 10 000 | **7.11** | **6.43** | 6.64 | 6.28 |
| 20 000 | 14.47¹ | 12.38¹ | ~19.7¹（有卡顿） | ~15.9¹ |

10k 拆分（ms）：SFML 当前 = build 2.82 + submit 0.66 + **GPU 3.04**；外提后 build 2.10；GL instanced = build 1.05 + submit 1.02 + **GPU 3.90**；GL stream = 2.54 + 0.46 + **GPU 2.78**。

> 核显上 **GPU 光栅化（4×MSAA 填充）才是大头**，SFML 的 CPU 提交只有 0.5–0.8 ms。instancing 在 10k 甚至比 SFML 多 0.9 ms GPU 时间（20k 时更明显），属于负优化方向。¹ 20k 数据来自 180 帧均值，核显长跑有明显抖动（p99 15–36 ms），仅供参考。

### 2.2 GTX 1650——整帧 avg ms（含约 2.5 ms 的 Optimus 呈现底噪）

| 船数 | SFML 逐顶点 | SFML 外提 | GL 3.3 instanced | GL 3.3 stream |
|---:|---:|---:|---:|---:|
| 1 000 | 3.20 | 3.06 | 3.11 | 2.68 |
| 5 000 | 2.82 | 3.15 | 2.55 | 3.22 |
| 10 000 | 3.85 | 3.44 | 3.18 | 6.47 |
| 20 000 | 9.20² | 4.23² | 4.30² | 9.57² |

10k 的**实体 pass 成本 = build + submit + GPU**（去掉底噪，才是可比的渲染成本）：

| 后端 | build | submit | GPU | 合计 |
|---|---:|---:|---:|---:|
| SFML 当前 | 2.18 | 1.02 | 0.22 | **3.42 ms** |
| SFML 外提 | 1.54 | 1.23 | 0.22 | **2.99 ms** |
| GL 3.3 instanced | 0.95 | 0.45 | 0.40 | **1.80 ms** |

> 独显上 SFML 的 GPU 只有 **0.22 ms**，成本几乎全在 CPU：逐顶点三角函数 2.2 ms + 固定管线客户端数组提交 1.0–2.2 ms。raw GL 能把这部分降到约 1.4 ms，**净省 1.6 ms/帧（10k）**，但整帧只从 3.4 降到 3.2（被底噪/呈现吃掉）。  
> ² 20k 帧时间受呈现与队列影响，p95 27–45 ms，不用于结论；10k 与 5k 的 100–180 帧均值更稳。

### 2.3 小行星 sprite（360 颗，上限值）

| 方案 | Intel 帧/提交 (ms) | NVIDIA 帧 (ms) |
|---|---:|---:|
| 每颗独立 `sf::Texture`＋每帧新建 sprite（当前旧做法） | **2.90** / submit 2.06 | 3.31 |
| 共享 1 张纹理（仍 360 次 draw） | 1.64 | 2.97 |
| **图集 + `setTextureRect`（仍 360 次 draw）** | **1.59** | 2.80 |
| raw GL 图集 + 1 次 instanced draw | 1.45 | 2.66 |

> 360 颗的瓶颈是**纹理绑定/逐 sprite draw**，不是 draw call 数量本身：图集一步就省 **1.3 ms（核显）**；再上 GL instancing 只多省 **0.14 ms**。主线已经做了图集，**这一项到此为止，不需要再改实例化**。

---

## 3. 结论：不换

1. **在游戏实际运行的核显上，SFML 批量路径和 raw GL 在 1k–10k 处于同一水平（±10%）**；10k 时 7.11 vs 6.28–6.64 ms，差异落在测量噪声内，且 20k 时 GL instancing 更慢。
2. **GPU 时间证明 SFML 的批次本身不低效**：独显 10k 时 SFML 只花 0.22 ms GPU；核显的 3 ms 是 4×MSAA 填充，换 API 也省不掉。
3. **真正的 CPU 成本是几何生成**（每船 6 次 `Vec2::rotated` = 12 次 sin/cos）。外提 sin/cos 一行改动即省：核显 10k −0.7 ms、独显 10k −0.6 ms，零风险。
4. **换 GL 的收益上限只有约 1.6 ms/帧（独显 10k，含光晕双 pass）**；主线现在关闭光晕/视野圈后实体 pass 更小，收益进一步减半。10k 混战 60 fps 已被 SFML 达成，说明渲染不是瓶颈。
5. **成本与风险**：shader/VAO/函数加载、与 SFML 的状态缓存共存（文字混合/矩阵）、上下文版本管理、核显上 instancing 反而更慢的驱动差异、双 GPU 路径回归测试。为 1.6 ms 引入这些不划算。

**建议顺序**：① 保留 SFML；② 若 profiler 显示 `drawShips` 仍靠前，先外提三角函数/复用 `shipBody_` 容量（预计 −0.6~0.7 ms）；③ 光晕继续可关；④ 小行星图集已做，停止；⑤ 只有当实体 pass 再次成为第一热点且 ① 用尽时，才考虑下面的 RendererGL。

---

## 4. 若将来真要换：RendererGL 集成设计（已验证可行）

- **上下文**：`Game::Game()` 里 `sf::ContextSettings s; s.majorVersion=3; s.minorVersion=3; s.attributeFlags=Default;`（兼容 profile，不是 Core）。实测 SFML 自己的固定管线在 3.3 兼容上下文里照常工作（`bench.exe --api sfml --ctx 33` 与 1.1 上下文帧时间一致），所以**同一个窗口**即可，无需第二个上下文。
- **分工**：`sf::RenderWindow` 继续负责窗口/输入/文字/UI；实体层由 `RendererGL` 用 raw GL 画。
- **每帧顺序**：`window.clear()` → GL 实体 pass（ships 三角、光晕、光点/拖尾/激光四边形、小行星图集，全部 instanced）→ **`window.resetGLStates()`** → SFML 文字/UI → `display()`。`resetGLStates()` 实测 **0.7–1.4 µs/次**，可忽略。
- **shader**：一个 instanced 三角形程序（`layout(location)` 属性：pos/rot/scale/color，`gl_VertexID` 选 3 个角）+ 一个 instanced 四边形程序（pos/rot/size/uvRect/color，纹理图集）。混合：体 `SRC_ALPHA,ONE_MINUS_SRC_ALPHA`，光晕/光点/拖尾/激光 `SRC_ALPHA,ONE`。
- **数据**：每实例 32 B（三角形）/48 B（四边形）；每帧 `BufferData(NULL)` 孤儿化 + `BufferSubData` 一次上传。10k 船 ≈ 1.3 MB/帧。
- **状态卫生**：GL pass 结束前 `UseProgram(0)`、`BindVertexArray(0)`、`BindBuffer(0)`、`BindTexture(0)`，再 `resetGLStates()`，否则 SFML 的 `StatesCache` 会认为混合/纹理仍是它上次设置的值，文字会画错。
- **小行星**：保留图集，per-instance UV rect；不要回到「一颗一纹理」。
- **估算成本**：新类 + 2 个 shader + 状态封装 + 三档回归 ≈ **1–2 人日**；之后每个新特效都要写 GL 路径，长期维护成本明显高于 SFML。

## 5. 风险与未验证项

- 隐藏窗口 + Optimus 使独显整帧含约 2.5 ms 呈现底噪；本报告用 `build+submit+GPU` 作为实体成本，整帧表仅作参考。
- 核显 20k 抖动大（p99 15–36 ms），未做温控/长时间稳定性测试。
- 未测：光点/拖尾/激光/视野圈的实际满负载混合场景（主线已关视野/光晕）；未测 AMD 显卡；未验证 4×MSAA 关闭后的收益（核显上预计明显，但会牺牲画质）。
- 未改动主工作区；所有实验代码在 `worktrees\gl\bench\`，原始输出在 `worktrees\gl\results\`（`q_sfml_intel.txt`、`q_gl_intel.txt`、`q_sfml_nv.txt`、`q_gl_nv.txt` 为带 GPU timer 的最终数据）。
