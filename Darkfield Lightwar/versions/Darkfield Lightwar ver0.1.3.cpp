//
// 游戏名 暗域光战 - Darkfield Lightwar
// 
// 开发环境配置：
// 1. Visual Studio 2022
// 2. SFML 2.6.2 库
// 3. C++17 标准
// 4. 字符编码：UTF-8 with BOM
// 5. 命令行：/utf-8
// 6. 字体文件：fonts/LXGWWenKaiTC-Light.ttf, fonts/msyh.ttc, fonts/SourceHanSans-Light.ttf, 
//    fonts/SmileySans-Oblique.otf, fonts/arial.ttf
// 
// 
// 最近一次更新：
// 1. 岩石损失像素时每个像素有0.05概率产生光点
// 2. 飞船新增恢复力、采集专精属性
// 3. 机动性影响旋转、加速和制动
// 4. 新增五种区域
// 5. 飞船看不到窗口外的物体，将窗口边缘视为障碍物
// 6. 重写避障模式和空闲模式
// 7. 新增飞船等级系统
// 8. 改进区域标识外观
// 9. 新增视野显示模式系统
// 10. 改进增援区域复制机制
// 11. 新增功能：鼠标点击选中飞船显示详细信息
// 12. 新增功能：L键开关等级显示
// 13. 新增功能：O键开关叛乱系统
// 14. 优化等级显示性能
// 15. 新增屏幕外静止物体清理机制
// 16. 新增两种特殊小行星：极速小行星和引力小行星
// 17. 扩大事件边界，确保大型小行星生成时不在窗口内
// 18. 彻底简化光点系统：移除收集状态，瞬间收集，无碰撞
// 19. 优化小行星渲染性能：使用顶点数组批量绘制
// 20. 新增：飞船升级冷却系统（5秒冷却时间）
// 21. 新增：禁生成区外物体优化，不进行碰撞检测和绘制
// 22. 增强小行星坚固性：质量与体积成正比，血量与质量^(2/3)成正比
// 23. 改进极速小行星拖尾：渐变四边形，尾端逐渐缩小，彗星白颜色
// 24. 新增：小行星缓冲系统，减少碰撞伤害
// 25. 新增：小行星碎裂时碎片随机方向运动，遵循动量守恒
//
// 
// ====================== 彻底隐藏控制台窗口 ======================
// 使用两种方法确保控制台完全隐藏：
// 1. 链接器选项：/subsystem:windows /ENTRY:mainCRTStartup
// 2. Windows API调用：FreeConsole()
#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#define NOMINMAX
#include <windows.h>
#endif

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <vector>
#include <cmath>
#include <random>
#include <memory>
#include <algorithm>
#include <queue>
#include <list>
#include <map>
#include <set>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <locale>
#include <codecvt>
#include <array>
#include <unordered_map>

// ====================== 游戏常量 ======================
constexpr float PI = 3.14159265358979323846f;
constexpr float TIME_STEP = 1.0f / 60.0f;    // 固定时间步长，确保物理稳定
constexpr int WINDOW_WIDTH = 1600;           // 窗口宽度
constexpr int WINDOW_HEIGHT = 900;           // 窗口高度
constexpr int EVENT_BOUNDARY_WIDTH = 2400;       // 事件边界宽度（扩大）
constexpr int EVENT_BOUNDARY_HEIGHT = 1700;      // 事件边界高度（扩大）
constexpr float RESOURCE_TO_BUILD = 10.0f;   // 建造新飞船所需资源（已修改为10）
constexpr float BUILD_TIME = 10.0f;          // 建造时间（秒）
constexpr float REINFORCEMENT_ZONE_INTERVAL = 6.0f; // 区域生成间隔（6-30秒随机）
constexpr float REINFORCEMENT_ZONE_DURATION = 16.0f; // 增援区域持续时间
constexpr float UPGRADE_ZONE_DURATION = 16.0f; // 升级区域持续时间
// 新增常量
constexpr float NO_GEN_ZONE_MARGIN = 200.0f;   // 禁生成区边界
constexpr float SINGULARITY_UPDATE_TIME = 30.0f; // 奇点更新间隔
constexpr float ASTEROID_SPAWN_DISTANCE = 1000.0f; // 小行星生成距离
constexpr float ASTEROID_CLOSE_SPAWN_RADIUS = 300.0f; // 近处生成半径
// ====================== 全局变量 ======================
int g_visionDisplayMode = 0;  // 视野显示模式：0-5
bool g_showLevel = false;     // 是否显示等级，默认为false
bool g_rebellionEnabled = true; // 叛乱系统是否开启，默认为true

// ====================== 前向声明 ======================
class PhysicsObject;
class LightPoint;
class Spaceship;
class Asteroid;
class LaserBeam;
class UpgradeZone;
class IPhysicsSystem; // 接口类

// ====================== 接口定义 ======================
class IPhysicsSystem {
public:
    virtual ~IPhysicsSystem() = default;

    // 光点相关
    virtual void addLightPoint(std::shared_ptr<LightPoint> lightPoint) = 0;
    virtual const std::vector<std::shared_ptr<LightPoint>>& getLightPoints() const = 0;
    virtual std::vector<std::shared_ptr<LightPoint>> getAllLightPoints() const = 0;

    // 激光相关
    virtual void addLaser(const LaserBeam& laser) = 0;

    // 资源相关
    virtual float getFactionResource(int faction) const = 0;
    virtual void setFactionResource(int faction, float amount) = 0;

    // 对象查询
    virtual std::vector<std::shared_ptr<PhysicsObject>> getObjectsInWindow() const = 0;
    virtual std::vector<std::shared_ptr<PhysicsObject>> getAllObjects() const = 0;

    // 检查对象是否在窗口内
    virtual bool isObjectInWindow(const std::shared_ptr<PhysicsObject>& obj) const = 0;

    // 获取所有飞船（新增）
    virtual std::vector<std::shared_ptr<Spaceship>> getAllSpaceships() const = 0;
};

// ====================== 数学工具类 ======================
struct Vector2D {
    float x, y;

    Vector2D(float x = 0, float y = 0) : x(x), y(y) {}

    // 向量运算
    Vector2D operator+(const Vector2D& other) const { return Vector2D(x + other.x, y + other.y); }
    Vector2D operator-(const Vector2D& other) const { return Vector2D(x - other.x, y - other.y); }
    Vector2D operator*(float scalar) const { return Vector2D(x * scalar, y * scalar); }
    Vector2D operator/(float scalar) const { return Vector2D(x / scalar, y / scalar); }
    Vector2D operator-() const { return Vector2D(-x, -y); }

    // 比较运算符
    bool operator==(const Vector2D& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Vector2D& other) const { return !(*this == other); }
    bool operator<(const Vector2D& other) const { return x < other.x || (x == other.x && y < other.y); }

    // 点积
    float dot(const Vector2D& other) const { return x * other.x + y * other.y; }

    // 叉积（返回标量）
    float cross(const Vector2D& other) const { return x * other.y - y * other.x; }

    // 向量长度
    float length() const { return std::sqrt(x * x + y * y); }

    // 向量长度平方（优化用，避免开方）
    float lengthSquared() const { return x * x + y * y; }

    // 规范化
    Vector2D normalized() const {
        float len = length();
        if (len > 0.00001f) return Vector2D(x / len, y / len);
        return Vector2D(0, 0);
    }

    // 旋转向量
    Vector2D rotated(float angle) const {
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);
        return Vector2D(x * cosA - y * sinA, x * sinA + y * cosA);
    }

    // 获取垂直向量
    Vector2D perpendicular() const {
        return Vector2D(-y, x);
    }

    // 与SFML向量转换
    sf::Vector2f toSFML() const { return sf::Vector2f(x, y); }

    // 获取角度（从x轴正方向开始，逆时针为正）
    float angle() const {
        return std::atan2(y, x);
    }

    // 获取角度差（返回[-PI, PI]范围内的差值）
    static float angleDifference(float a, float b) {
        float diff = b - a;
        while (diff > PI) diff -= 2.0f * PI;
        while (diff < -PI) diff += 2.0f * PI;
        return diff;
    }
};

// ====================== 碰撞检测辅助结构 ======================
struct Projection {
    float min, max;

    bool overlaps(const Projection& other) const {
        return !(min > other.max || other.min > max);
    }

    float getOverlap(const Projection& other) const {
        return std::min(max, other.max) - std::max(min, other.min);
    }
};

// ====================== UI字体类 ======================
class GameFont {
private:
    sf::Font font;
    bool loaded;

public:
    GameFont() : loaded(false) {
        // 尝试加载多种字体文件，确保中文字体能正常显示
        // 使用UTF-8编码的文件名
        std::vector<std::string> fontPaths = {
            "fonts/LXGWWenKaiTC-Light.ttf",   // 霞鹜文楷
            "fonts/msyh.ttc",                  // 微软雅黑
            "fonts/SourceHanSans-Light.ttf",   // 思源黑体
            "fonts/SmileySans-Oblique.otf",    // 得意黑
            "fonts/arial.ttf"                  // Arial（备用）
        };

        for (const auto& path : fontPaths) {
            if (font.loadFromFile(path)) {
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            // 如果所有字体文件加载失败，尝试使用系统默认字体
            loaded = font.loadFromFile("C:/Windows/Fonts/msyh.ttc");
        }
    }

    const sf::Font& getFont() const { return font; }
    bool isLoaded() const { return loaded; }
};

// ====================== 像素方块类 ======================
class PixelBlock {
private:
    Vector2D localPosition;
    bool active;
    sf::Color color;
    float size;

public:
    PixelBlock(Vector2D localPos, float pixelSize, sf::Color col)
        : localPosition(localPos), active(true), color(col), size(pixelSize) {
    }

    // 获取顶点数据（用于批量绘制）
    void getQuadVertices(Vector2D parentPos, float parentRotation, sf::Vertex* vertices) const {
        if (!active) return;

        // 旋转后的位置
        Vector2D rotatedPos = localPosition.rotated(parentRotation);
        Vector2D worldPos = parentPos + rotatedPos;

        // 计算四个角点
        float halfSize = size / 2.0f;

        // 如果不需要旋转，直接计算
        if (parentRotation == 0) {
            vertices[0].position = sf::Vector2f(worldPos.x - halfSize, worldPos.y - halfSize);
            vertices[1].position = sf::Vector2f(worldPos.x + halfSize, worldPos.y - halfSize);
            vertices[2].position = sf::Vector2f(worldPos.x + halfSize, worldPos.y + halfSize);
            vertices[3].position = sf::Vector2f(worldPos.x - halfSize, worldPos.y + halfSize);
        }
        else {
            // 需要旋转时，先计算未旋转的四个角点，然后旋转
            float cosA = std::cos(parentRotation);
            float sinA = std::sin(parentRotation);

            // 四个角的本地坐标
            Vector2D corners[4] = {
                Vector2D(-halfSize, -halfSize),
                Vector2D(halfSize, -halfSize),
                Vector2D(halfSize, halfSize),
                Vector2D(-halfSize, halfSize)
            };

            for (int i = 0; i < 4; i++) {
                Vector2D rotatedCorner = Vector2D(
                    corners[i].x * cosA - corners[i].y * sinA,
                    corners[i].x * sinA + corners[i].y * cosA
                );
                vertices[i].position = (worldPos + rotatedCorner).toSFML();
            }
        }

        // 设置颜色
        for (int i = 0; i < 4; i++) {
            vertices[i].color = color;
        }
    }

    // 获取世界坐标下的包围盒
    sf::FloatRect getWorldBounds(Vector2D parentPos, float parentRotation = 0) const {
        Vector2D rotatedPos = localPosition.rotated(parentRotation);
        sf::Vector2f worldPos(parentPos.x + rotatedPos.x - size / 2,
            parentPos.y + rotatedPos.y - size / 2);
        return sf::FloatRect(worldPos, sf::Vector2f(size, size));
    }

    // 检查点是否在像素块内
    bool contains(Vector2D worldPoint, Vector2D parentPos, float parentRotation = 0) const {
        Vector2D rotatedPos = localPosition.rotated(parentRotation);
        Vector2D blockCenter = parentPos + rotatedPos;
        Vector2D delta = worldPoint - blockCenter;
        return std::abs(delta.x) <= size / 2 && std::abs(delta.y) <= size / 2;
    }

    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }
    Vector2D getLocalPosition() const { return localPosition; }
    float getDistance() const { return localPosition.length(); }
    sf::Color getColor() const { return color; }
    float getSize() const { return size; }

    Vector2D getWorldPosition(Vector2D parentPos, float parentRotation = 0) const {
        return parentPos + localPosition.rotated(parentRotation);
    }
};

// ====================== 渲染批处理系统 ======================
class RenderBatchSystem {
private:
    // 批量渲染数据
    struct BatchData {
        sf::VertexArray vertices;
        int objectCount;
        bool needsUpdate;

        BatchData() : vertices(sf::Quads), objectCount(0), needsUpdate(false) {}
    };

    std::unordered_map<const void*, BatchData> asteroidBatches;

public:
    // 更新小行星的批量渲染数据
    void updateAsteroidBatch(const void* asteroidId, const std::vector<std::shared_ptr<PixelBlock>>& pixelBlocks,
        Vector2D position, float rotation) {
        auto& batch = asteroidBatches[asteroidId];
        batch.needsUpdate = true;

        // 计算需要的顶点数量（每个像素块4个顶点）
        size_t vertexCount = 0;
        for (const auto& pixel : pixelBlocks) {
            if (pixel->isActive()) {
                vertexCount += 4;
            }
        }

        // 重新设置顶点数组大小
        batch.vertices.clear();
        batch.vertices.resize(vertexCount);

        // 填充顶点数据
        size_t vertexIndex = 0;
        for (const auto& pixel : pixelBlocks) {
            if (pixel->isActive()) {
                pixel->getQuadVertices(position, rotation, &batch.vertices[vertexIndex]);
                vertexIndex += 4;
            }
        }

        batch.objectCount = static_cast<int>(vertexCount / 4);
        batch.needsUpdate = false;
    }

    // 绘制小行星批次
    void drawAsteroidBatch(const void* asteroidId, sf::RenderWindow& window) {
        auto it = asteroidBatches.find(asteroidId);
        if (it != asteroidBatches.end() && it->second.objectCount > 0) {
            window.draw(it->second.vertices);
        }
    }

    // 移除批次数据
    void removeBatch(const void* asteroidId) {
        asteroidBatches.erase(asteroidId);
    }

    // 清理所有批次数据
    void clear() {
        asteroidBatches.clear();
    }
};

// 全局渲染批处理系统
static RenderBatchSystem g_renderBatchSystem;

// ====================== 物理基类 ======================
class PhysicsObject : public std::enable_shared_from_this<PhysicsObject> {
protected:
    Vector2D position;
    Vector2D velocity;
    Vector2D acceleration;
    float mass;
    float health;
    float maxHealth;
    float restitution;
    bool isActive;
    float rotation;
    float angularVelocity;
    int factionId; // 阵营ID，-1表示无阵营

public:
    PhysicsObject(Vector2D pos, float m, float h, int faction = -1)
        : position(pos), velocity(0, 0), acceleration(0, 0),
        mass(m), health(h), maxHealth(h), restitution(0.2f),
        isActive(true), rotation(0), angularVelocity(0), factionId(faction) {
    }

    virtual ~PhysicsObject() = default;

    // 更新物理状态
    virtual void update(float deltaTime) {
        if (!isActive) return;

        // 无阻力太空物理
        velocity = velocity + acceleration * deltaTime;
        position = position + velocity * deltaTime;
        rotation += angularVelocity * deltaTime;

        // 规范化旋转角度到[0, 2π)
        while (rotation >= 2.0f * PI) rotation -= 2.0f * PI;
        while (rotation < 0) rotation += 2.0f * PI;

        // 重置加速度
        acceleration = Vector2D(0, 0);
    }

    // 应用力（牛顿第二定律）
    void applyForce(const Vector2D& force) {
        acceleration = acceleration + (force / mass);
    }

    // 应用扭矩
    void applyTorque(float torque, float momentOfInertia) {
        angularVelocity += torque / momentOfInertia * TIME_STEP;
    }

    // 虚函数：获取精确碰撞像素块
    virtual std::vector<std::shared_ptr<PixelBlock>> getPixelBlocks() const = 0;

    // 虚函数：获取凸包顶点（用于激光碰撞检测）
    virtual std::vector<Vector2D> getConvexHull() const = 0;

    // 虚函数：获取外接圆半径
    virtual float getBoundingRadius() const = 0;

    // 处理伤害
    virtual void takeDamage(float damage, Vector2D damagePoint = Vector2D(0, 0), std::shared_ptr<PhysicsObject> attacker = nullptr) {
        if (!isActive) return;

        health -= damage;
        if (health <= 0) {
            health = 0;
            isActive = false;
        }
    }

    // 销毁时的效果
    virtual std::vector<std::shared_ptr<PhysicsObject>> onDestroy() {
        return {}; // 默认返回空列表
    }

    // 生成光点
    virtual std::vector<std::shared_ptr<class LightPoint>> generateLightPoints() {
        return {}; // 默认不生成光点
    }

    // 绘制函数
    virtual void draw(sf::RenderWindow& window) = 0;

    // 获取对象类型标识
    virtual std::string getType() const = 0;

    // 检查是否可以被采集（光点）
    virtual bool isCollectible() const { return false; }

    // 开始采集（如果是可采集对象）
    virtual void startCollecting(std::shared_ptr<PhysicsObject> collector) {}

    // 结束采集
    virtual void stopCollecting() {}

    // 获取采集进度
    virtual float getCollectProgress() const { return 0.0f; }

    // 获取像素数量（用于资源计算）
    virtual int getPixelCount() const { return 0; }

    // 检查是否在禁生成区内（优化用）
    virtual bool isInNoGenZone() const {
        return position.x >= -NO_GEN_ZONE_MARGIN &&
            position.x <= WINDOW_WIDTH + NO_GEN_ZONE_MARGIN &&
            position.y >= -NO_GEN_ZONE_MARGIN &&
            position.y <= WINDOW_HEIGHT + NO_GEN_ZONE_MARGIN;
    }

    // Getter & Setter
    Vector2D getPosition() const { return position; }
    Vector2D getVelocity() const { return velocity; }
    float getMass() const { return mass; }
    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    float getRestitution() const { return restitution; }
    bool getIsActive() const { return isActive; }
    float getRotation() const { return rotation; }
    float getAngularVelocity() const { return angularVelocity; }
    int getFactionId() const { return factionId; }
    void setVelocity(const Vector2D& v) { velocity = v; }
    void addVelocity(const Vector2D& v) { velocity = velocity + v; }
    void setPosition(const Vector2D& p) { position = p; }
    void setRestitution(float r) { restitution = std::min(std::max(r, 0.0f), 1.0f); }
    void setRotation(float r) { rotation = r; }
    void setAngularVelocity(float av) { angularVelocity = av; }
    void setFactionId(int id) { factionId = id; }
    void setHealth(float h) { health = h; }
    void setMaxHealth(float h) { maxHealth = h; }

    // 检查是否飞出事件边界
    virtual bool isOutOfBounds() {
        float margin = 100.0f;
        return (position.x < -margin || position.x > EVENT_BOUNDARY_WIDTH + margin ||
            position.y < -margin || position.y > EVENT_BOUNDARY_HEIGHT + margin);
    }

    // 检查是否在视野内
    virtual bool isInView(const Vector2D& viewerPos, float viewRange) const {
        float distSq = (position - viewerPos).lengthSquared();
        return distSq <= viewRange * viewRange;
    }

    // 检查是否在扇形攻击范围内
    virtual bool isInAttackRange(const Vector2D& attackerPos, float attackerRotation, float attackRange, float attackAngle) const {
        Vector2D toTarget = position - attackerPos;
        float distance = toTarget.length();
        if (distance > attackRange) return false;

        float targetAngle = toTarget.angle();
        float angleDiff = Vector2D::angleDifference(attackerRotation, targetAngle);

        return std::abs(angleDiff) <= attackAngle;
    }

    // 检查是否在窗口内
    virtual bool isInWindow() const {
        return position.x >= 0 && position.x <= WINDOW_WIDTH &&
            position.y >= 0 && position.y <= WINDOW_HEIGHT;
    }

    // 检查是否在窗口外且静止或超低移速（新增）
    virtual bool isOutOfWindowAndStatic(float speedThreshold = 0.01f) const {
        bool isOutOfWindow = !isInWindow();
        bool isStatic = velocity.length() < speedThreshold;
        return isOutOfWindow && isStatic;
    }
};

// ====================== 简化后的光点类 ======================
class LightPoint : public PhysicsObject {
private:
    sf::Color color;
    float radius;
    sf::CircleShape shape;

public:
    LightPoint(Vector2D pos, Vector2D vel, sf::Color col)
        : PhysicsObject(pos, 0.5f, 1.0f, -1), color(col), radius(2.0f) {
        velocity = vel;

        shape.setRadius(radius);
        shape.setFillColor(color);
        shape.setOutlineThickness(0);
        shape.setOrigin(radius, radius);
    }

    void update(float deltaTime) override {
        if (!isActive) return;

        // 轻微阻力
        velocity = velocity * 0.98f;
        position = position + velocity * deltaTime;
        shape.setPosition(position.toSFML());
    }

    // 光点不参与碰撞检测，返回空列表
    std::vector<std::shared_ptr<PixelBlock>> getPixelBlocks() const override {
        return {};
    }

    // 光点不参与碰撞检测，返回空凸包
    std::vector<Vector2D> getConvexHull() const override {
        return {};
    }

    // 光点不参与碰撞检测，返回0
    float getBoundingRadius() const override {
        return 0;
    }

    void draw(sf::RenderWindow& window) override {
        if (isActive) {
            shape.setPosition(position.toSFML());
            window.draw(shape);

            // 添加发光效果
            sf::CircleShape glow(radius * 1.5f);
            glow.setFillColor(sf::Color(color.r, color.g, color.b, 50));
            glow.setOrigin(glow.getRadius(), glow.getRadius());
            glow.setPosition(position.toSFML());
            window.draw(glow);
        }
    }

    std::string getType() const override { return "LightPoint"; }

    // 光点总是可以被采集
    bool isCollectible() const override { return true; }

    // 开始收集（瞬间完成）
    void startCollecting(std::shared_ptr<PhysicsObject> collector) override {
        isActive = false; // 立即标记为不活跃，表示已收集
    }

    // 获取收集进度（瞬间收集，总是返回1）
    float getCollectProgress() const override {
        return isActive ? 0.0f : 1.0f;
    }

    // 获取像素数量（光点价值为1）
    int getPixelCount() const override { return 1; }

    // 获取颜色
    sf::Color getColor() const { return color; }
};

// ====================== 激光类 ======================
class LaserBeam {
private:
    Vector2D startPos;
    Vector2D endPos;
    sf::Color color;
    float lifeTime;
    float maxLifeTime;
    float thickness;
    float damage;
    std::weak_ptr<PhysicsObject> owner; // 激光所有者，使用weak_ptr避免循环引用
    bool hasHit; // 是否已经击中目标

public:
    LaserBeam(Vector2D start, Vector2D end, sf::Color col, std::shared_ptr<PhysicsObject> own, float dmg = 10.0f, float duration = 0.1f)
        : startPos(start), endPos(end), color(col), maxLifeTime(duration), thickness(2.5f), damage(dmg), owner(own), hasHit(false) {
        lifeTime = maxLifeTime;
        color.a = 220;
    }

    void update(float deltaTime) {
        lifeTime -= deltaTime;
    }

    void draw(sf::RenderWindow& window) const {
        if (lifeTime <= 0) return;

        // 绘制激光核心（亮线）
        sf::Vertex line[] = {
            sf::Vertex(startPos.toSFML(), color),
            sf::Vertex(endPos.toSFML(), color)
        };

        // 绘制光晕效果
        sf::Color glowColor = color;
        glowColor.a = 100;

        Vector2D dir = (endPos - startPos).normalized();
        Vector2D perp = dir.perpendicular() * thickness;

        sf::VertexArray glowQuad(sf::TriangleStrip, 4);  // 修复：使用正确的枚举值
        glowQuad[0].position = (startPos - perp).toSFML();
        glowQuad[1].position = (startPos + perp).toSFML();
        glowQuad[2].position = (endPos - perp).toSFML();
        glowQuad[3].position = (endPos + perp).toSFML();

        glowQuad[0].color = glowColor;
        glowQuad[1].color = glowColor;
        glowQuad[2].color = glowColor;
        glowQuad[3].color = glowColor;

        window.draw(glowQuad);
        window.draw(line, 2, sf::Lines);

        // 绘制激光终点的光球（增加命中判定范围）
        sf::CircleShape endGlow(12.0f);  // 光球半径增加到12
        endGlow.setFillColor(sf::Color(color.r, color.g, color.b, 120));
        endGlow.setOrigin(12.0f, 12.0f);
        endGlow.setPosition(endPos.toSFML());
        window.draw(endGlow);
    }

    bool isAlive() const { return lifeTime > 0; }
    Vector2D getStart() const { return startPos; }
    Vector2D getEnd() const { return endPos; }
    float getDamage() const { return damage; }
    std::shared_ptr<PhysicsObject> getOwner() const { return owner.lock(); }
    bool getHasHit() const { return hasHit; }
    void setHasHit(bool hit) { hasHit = hit; }
    void setEndPos(const Vector2D& end) { endPos = end; }
};

// ====================== 区域基类 ======================
class UpgradeZone {
protected:
    Vector2D position;
    float lifetime; // 当前剩余时间
    float maxLifetime; // 最大生存时间
    float currentRadius; // 当前半径
    float maxRadius; // 最大半径
    bool active;
    int zoneType; // 0=增援, 1=生存, 2=进攻, 3=机动, 4=生产, 5=领域

    // 装饰元素
    sf::CircleShape zoneCircle;
    float rotationAngle; // 旋转角度

    // 虚线效果相关
    std::vector<sf::VertexArray> dashedLines;
    float dashLength;
    float gapLength;

public:
    UpgradeZone(Vector2D pos, int type, float duration = UPGRADE_ZONE_DURATION)
        : position(pos), maxLifetime(duration), lifetime(duration), active(true),
        maxRadius(100.0f), currentRadius(5.0f), zoneType(type), rotationAngle(0),
        dashLength(10.0f), gapLength(5.0f) {

        // 初始化虚线圆圈
        updateDashedCircle();

        // 根据类型设置颜色
        sf::Color zoneColor;
        switch (zoneType) {
        case 0: zoneColor = sf::Color::White; break;      // 增援 - 白色
        case 1: zoneColor = sf::Color(255, 100, 100, 200); break; // 生存 - 红色
        case 2: zoneColor = sf::Color(255, 255, 100, 200); break; // 进攻 - 黄色
        case 3: zoneColor = sf::Color(100, 255, 100, 200); break; // 机动 - 绿色
        case 4: zoneColor = sf::Color(100, 150, 255, 200); break; // 生产 - 蓝色
        case 5: zoneColor = sf::Color(200, 100, 255, 200); break; // 领域 - 紫色
        default: zoneColor = sf::Color::White; break;
        }

        zoneCircle.setRadius(currentRadius);
        zoneCircle.setFillColor(sf::Color::Transparent);
        zoneCircle.setOutlineColor(zoneColor);
        zoneCircle.setOutlineThickness(2.0f);
        zoneCircle.setOrigin(currentRadius, currentRadius);
    }

    virtual void update(float deltaTime) {
        if (!active) return;

        lifetime -= deltaTime;

        if (lifetime <= 0) {
            active = false;
            return;
        }

        // 根据时间阶段调整半径
        float phaseTime = maxLifetime - lifetime; // 已过时间

        if (phaseTime <= 4.0f) {
            // 变大阶段
            float t = phaseTime / 4.0f;
            currentRadius = 5.0f + (maxRadius - 5.0f) * t;
        }
        else if (phaseTime <= 12.0f) {
            // 保持阶段
            currentRadius = maxRadius;
        }
        else {
            // 缩小阶段
            float t = (phaseTime - 12.0f) / 4.0f;
            currentRadius = maxRadius * (1.0f - t);
        }

        // 更新虚线圆圈
        updateDashedCircle();

        // 更新旋转
        rotationAngle += deltaTime * 0.5f;
        if (rotationAngle > 2.0f * PI) rotationAngle -= 2.0f * PI;

        // 更新圆圈位置和大小
        zoneCircle.setRadius(currentRadius);
        zoneCircle.setOrigin(currentRadius, currentRadius);
        zoneCircle.setPosition(position.toSFML());
    }

    virtual void draw(sf::RenderWindow& window) const {
        if (!active) return;

        // 绘制虚线圆圈
        for (const auto& line : dashedLines) {
            window.draw(line);
        }

        // 绘制区域中心标识
        drawIndicator(window);
    }

    virtual void drawIndicator(sf::RenderWindow& window) const = 0;

    // 检查点是否在区域内
    bool contains(Vector2D point) const {
        float distance = (point - position).length();
        return distance <= currentRadius;
    }

    // 获取区域内随机位置
    Vector2D getRandomPosition() const {
        float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
        float distance = static_cast<float>(rand() % 100) / 100.0f * currentRadius * 0.8f;
        return position + Vector2D(std::cos(angle), std::sin(angle)) * distance;
    }

    bool isActive() const { return active; }
    Vector2D getPosition() const { return position; }
    float getRadius() const { return currentRadius; }
    float getLifetime() const { return lifetime; }
    int getZoneType() const { return zoneType; }

protected:
    // 更新虚线圆圈
    void updateDashedCircle() {
        dashedLines.clear();

        int segments = 48; // 圆圈分割段数
        float angleStep = 2.0f * PI / segments;

        for (int i = 0; i < segments; i++) {
            float startAngle = i * angleStep;
            float endAngle = (i + 1) * angleStep;

            // 创建虚线效果：每段间隔绘制
            if (i % 2 == 0) { // 每2段绘制1段，形成虚线效果
                Vector2D startPos = position + Vector2D(std::cos(startAngle), std::sin(startAngle)) * currentRadius;
                Vector2D endPos = position + Vector2D(std::cos(endAngle), std::sin(endAngle)) * currentRadius;

                sf::VertexArray line(sf::Lines, 2);
                line[0].position = startPos.toSFML();
                line[1].position = endPos.toSFML();

                // 根据类型设置颜色
                switch (zoneType) {
                case 0: line[0].color = line[1].color = sf::Color::White; break;
                case 1: line[0].color = line[1].color = sf::Color(255, 100, 100, 200); break;
                case 2: line[0].color = line[1].color = sf::Color(255, 255, 100, 200); break;
                case 3: line[0].color = line[1].color = sf::Color(100, 255, 100, 200); break;
                case 4: line[0].color = line[1].color = sf::Color(100, 150, 255, 200); break;
                case 5: line[0].color = line[1].color = sf::Color(200, 100, 255, 200); break;
                }

                dashedLines.push_back(line);
            }
        }
    }
};

// ====================== 增援区域类 ======================
class ReinforcementZone : public UpgradeZone {
private:
    sf::ConvexShape shipIndicator;

public:
    ReinforcementZone(Vector2D pos) : UpgradeZone(pos, 0, REINFORCEMENT_ZONE_DURATION) {
        // 初始化飞船标识（三角形）
        shipIndicator.setPointCount(3);
        shipIndicator.setPoint(0, sf::Vector2f(0, -8));
        shipIndicator.setPoint(1, sf::Vector2f(-5, 5));
        shipIndicator.setPoint(2, sf::Vector2f(5, 5));
        shipIndicator.setFillColor(sf::Color::White);
        shipIndicator.setOutlineColor(sf::Color(200, 200, 200));
        shipIndicator.setOutlineThickness(1);
        shipIndicator.setOrigin(0, 0);
    }

    void update(float deltaTime) override {
        UpgradeZone::update(deltaTime);

        if (!active) return;

        // 更新飞船标识位置（在圆圈内随机位置）
        float indicatorAngle = rotationAngle * 2.0f;
        float indicatorDistance = currentRadius * 0.7f;
        Vector2D indicatorOffset(std::cos(indicatorAngle), std::sin(indicatorAngle));
        indicatorOffset = indicatorOffset * indicatorDistance;

        shipIndicator.setPosition(position.x + indicatorOffset.x,
            position.y + indicatorOffset.y);
        shipIndicator.setRotation(rotationAngle * 180.0f / PI);
    }

    void drawIndicator(sf::RenderWindow& window) const override {
        if (!active) return;
        window.draw(shipIndicator);
    }
};

// ====================== 生存升级区域类 ======================
class SurvivalUpgradeZone : public UpgradeZone {
private:
    sf::RectangleShape crossHorizontal;
    sf::RectangleShape crossVertical;
    sf::RectangleShape crossDiagonal1;
    sf::RectangleShape crossDiagonal2;

public:
    SurvivalUpgradeZone(Vector2D pos) : UpgradeZone(pos, 1) {
        // 初始化红色十字（交错十字）
        crossHorizontal.setSize(sf::Vector2f(20, 4));
        crossVertical.setSize(sf::Vector2f(4, 20));
        crossHorizontal.setFillColor(sf::Color(255, 100, 100, 180));
        crossVertical.setFillColor(sf::Color(255, 100, 100, 180));
        crossHorizontal.setOrigin(10, 2);
        crossVertical.setOrigin(2, 10);

        // 初始化对角线
        crossDiagonal1.setSize(sf::Vector2f(16, 4));
        crossDiagonal2.setSize(sf::Vector2f(16, 4));
        crossDiagonal1.setFillColor(sf::Color(255, 100, 100, 180));
        crossDiagonal2.setFillColor(sf::Color(255, 100, 100, 180));
        crossDiagonal1.setOrigin(8, 2);
        crossDiagonal2.setOrigin(8, 2);
    }

    void update(float deltaTime) override {
        UpgradeZone::update(deltaTime);

        if (!active) return;

        // 设置十字位置
        crossHorizontal.setPosition(position.toSFML());
        crossVertical.setPosition(position.toSFML());
        crossDiagonal1.setPosition(position.toSFML());
        crossDiagonal2.setPosition(position.toSFML());

        // 缓慢旋转
        crossHorizontal.setRotation(rotationAngle * 180.0f / PI);
        crossVertical.setRotation(rotationAngle * 180.0f / PI + 90);

        // 对角线以相反方向旋转
        crossDiagonal1.setRotation(rotationAngle * 180.0f / PI + 45);
        crossDiagonal2.setRotation(rotationAngle * 180.0f / PI + 135);
    }

    void drawIndicator(sf::RenderWindow& window) const override {
        if (!active) return;
        window.draw(crossHorizontal);
        window.draw(crossVertical);
        window.draw(crossDiagonal1);
        window.draw(crossDiagonal2);
    }
};

// ====================== 进攻升级区域类 ======================
class AttackUpgradeZone : public UpgradeZone {
private:
    sf::CircleShape laserCircle;
    std::vector<sf::RectangleShape> laserLines;
    std::vector<float> lineAngles;

public:
    AttackUpgradeZone(Vector2D pos) : UpgradeZone(pos, 2) {
        // 初始化黄色激光标识
        laserCircle.setRadius(6);
        laserCircle.setFillColor(sf::Color(255, 255, 100, 180));
        laserCircle.setOrigin(6, 6);

        // 创建3条激光线
        laserLines.resize(3);
        lineAngles = { 0.0f, 120.0f, 240.0f };

        for (size_t i = 0; i < laserLines.size(); i++) {
            laserLines[i].setSize(sf::Vector2f(16, 4));
            laserLines[i].setFillColor(sf::Color(255, 255, 100, 180));
            laserLines[i].setOrigin(0, 2);
        }
    }

    void update(float deltaTime) override {
        UpgradeZone::update(deltaTime);

        if (!active) return;

        // 设置激光标识位置
        laserCircle.setPosition(position.toSFML());

        // 更新激光线
        for (size_t i = 0; i < laserLines.size(); i++) {
            float lineAngle = rotationAngle + lineAngles[i] * PI / 180.0f;
            laserLines[i].setPosition(position.toSFML());
            laserLines[i].setRotation(lineAngle * 180.0f / PI);
            laserLines[i].setSize(sf::Vector2f(currentRadius * 0.8f, 4));
        }
    }

    void drawIndicator(sf::RenderWindow& window) const override {
        if (!active) return;
        for (const auto& line : laserLines) {
            window.draw(line);
        }
        window.draw(laserCircle);
    }
};

// ====================== 机动升级区域类 ======================
class MobilityUpgradeZone : public UpgradeZone {
private:
    sf::ConvexShape arrowShape;
    sf::RectangleShape arrowLine;

public:
    MobilityUpgradeZone(Vector2D pos) : UpgradeZone(pos, 3) {
        // 初始化绿色箭头标识
        arrowShape.setPointCount(3);
        arrowShape.setPoint(0, sf::Vector2f(0, -8));
        arrowShape.setPoint(1, sf::Vector2f(-5, 0));
        arrowShape.setPoint(2, sf::Vector2f(5, 0));
        arrowShape.setFillColor(sf::Color(100, 255, 100, 180));
        arrowShape.setOrigin(0, 0);

        // 箭头柄 - 变粗变短
        arrowLine.setSize(sf::Vector2f(12, 5));
        arrowLine.setFillColor(sf::Color(100, 255, 100, 180));
        arrowLine.setOrigin(0, 2.5);
    }

    void update(float deltaTime) override {
        UpgradeZone::update(deltaTime);

        if (!active) return;

        // 设置箭头位置
        float arrowAngle = rotationAngle + PI * 0.5f;
        Vector2D arrowPos = position + Vector2D(std::cos(arrowAngle), std::sin(arrowAngle)) * (currentRadius * 0.6f);

        arrowShape.setPosition(arrowPos.toSFML());
        arrowShape.setRotation(arrowAngle * 180.0f / PI + 90);

        // 箭头线从中心指向箭头
        arrowLine.setPosition(position.toSFML());
        arrowLine.setRotation(arrowAngle * 180.0f / PI);
        arrowLine.setSize(sf::Vector2f(currentRadius * 0.6f, 5));
    }

    void drawIndicator(sf::RenderWindow& window) const override {
        if (!active) return;
        window.draw(arrowLine);
        window.draw(arrowShape);
    }
};

// ====================== 生产升级区域类 ======================
class ProductionUpgradeZone : public UpgradeZone {
private:
    sf::RectangleShape hammerHead;
    sf::RectangleShape hammerHandle;

public:
    ProductionUpgradeZone(Vector2D pos) : UpgradeZone(pos, 4) {
        // 初始化蓝色锤子标识
        hammerHead.setSize(sf::Vector2f(10, 8));
        hammerHead.setFillColor(sf::Color(100, 150, 255, 180));
        hammerHead.setOrigin(5, 4);

        // 锤子柄 - 变粗变短
        hammerHandle.setSize(sf::Vector2f(10, 6));
        hammerHandle.setFillColor(sf::Color(100, 150, 255, 180));
        hammerHandle.setOrigin(5, 3);
    }

    void update(float deltaTime) override {
        UpgradeZone::update(deltaTime);

        if (!active) return;

        // 设置锤子位置和旋转
        hammerHead.setPosition(position.toSFML());
        hammerHandle.setPosition(position.toSFML());

        hammerHead.setRotation(rotationAngle * 180.0f / PI);
        hammerHandle.setRotation(rotationAngle * 180.0f / PI);
    }

    void drawIndicator(sf::RenderWindow& window) const override {
        if (!active) return;
        window.draw(hammerHandle);
        window.draw(hammerHead);
    }
};

// ====================== 领域升级区域类 ======================
class DomainUpgradeZone : public UpgradeZone {
private:
    sf::CircleShape innerCircle;
    sf::CircleShape outerCircle;
    sf::CircleShape centerCircle;

public:
    DomainUpgradeZone(Vector2D pos) : UpgradeZone(pos, 5) {
        // 初始化紫色圆圈标识
        innerCircle.setRadius(6);
        innerCircle.setFillColor(sf::Color(200, 100, 255, 80));
        innerCircle.setOutlineColor(sf::Color(200, 100, 255, 180));
        innerCircle.setOutlineThickness(1);
        innerCircle.setOrigin(6, 6);

        outerCircle.setRadius(10);
        outerCircle.setFillColor(sf::Color::Transparent);
        outerCircle.setOutlineColor(sf::Color(200, 100, 255, 120));
        outerCircle.setOutlineThickness(1);
        outerCircle.setOrigin(10, 10);

        centerCircle.setRadius(3);
        centerCircle.setFillColor(sf::Color::Transparent);
        centerCircle.setOutlineColor(sf::Color::Transparent);
        centerCircle.setOutlineThickness(0);
        centerCircle.setOrigin(3, 3);
    }

    void update(float deltaTime) override {
        UpgradeZone::update(deltaTime);

        if (!active) return;

        // 设置圆圈位置
        innerCircle.setPosition(position.toSFML());
        outerCircle.setPosition(position.toSFML());
        centerCircle.setPosition(position.toSFML());

        // 内圈和外圈反向旋转
        innerCircle.setRotation(rotationAngle * 180.0f / PI);
        outerCircle.setRotation(-rotationAngle * 180.0f / PI);
    }

    void drawIndicator(sf::RenderWindow& window) const override {
        if (!active) return;
        window.draw(outerCircle);
        window.draw(innerCircle);
        window.draw(centerCircle);
    }
};

// ====================== 三角形飞船类 ======================
class Spaceship : public PhysicsObject {
private:
    sf::ConvexShape shape;
    sf::ConvexShape glowShape;
    sf::Color baseColor;
    sf::Color glowColor;
    float size;
    std::vector<Vector2D> localVertices;

    // 武器属性
    float fireRate;          // 每秒射击次数
    float fireTimer;
    float fireRange;         // 射程
    float damage;            // 单发伤害
    float fireAngle;         // 射击角度（弧度）
    std::vector<LaserBeam> lasers;

    // 机动属性
    float maneuverability;   // 机动性（影响旋转速度和加速度）
    float maxSpeed;          // 最大速度（移速阈值）
    float accelerationRate;  // 加速度
    float brakeRate;         // 制动速率

    // 新增属性
    float recoveryRate;      // 恢复力，初始为1，每秒回复1生命值
    float miningExpertise;   // 采集专精，初始为0.1，决定攻击岩石时产生光点的概率增量

    // AI状态
    enum class AIState {
        ATTACKING,      // 攻击敌对飞船
        COLLECTING,     // 采集光点
        MINING,         // 攻击小行星
        AVOIDING,       // 避障状态
        IDLE,           // 空闲状态
        TURNING         // 主动转向状态（防止被纠正）
    };
    AIState currentState;
    AIState previousState;   // 之前的状态

    // 目标对象
    std::weak_ptr<PhysicsObject> target;

    // 视野范围
    float viewRange;

    // 状态计时器
    float stateTimer;
    float idleActionTimer;

    // 建造系统
    bool isBuilding;          // 是否正在建造
    float buildProgress;      // 建造进度（0-1）
    float buildTimer;         // 建造计时器
    sf::CircleShape buildRing; // 建造环形指示器
    float buildSpeed;         // 建造速度（每秒进度增加量）

    // 采集相关
    float collectionDistance; // 收集距离（新增属性）

    // 最近一次决定的时间（用于随机决策）
    float lastDecisionTime;
    float decisionInterval;

    // 主动转向相关
    bool isTurningIntentionally;  // 是否正在主动转向
    float turningTime;           // 转向计时器
    float turningDuration;       // 转向持续时间

    // 物理系统接口引用
    IPhysicsSystem* physicsSystem;

    // 独立随机数生成器
    std::mt19937 randomEngine;
    std::uniform_real_distribution<float> randomDist;

    // 升级相关（移除baseXXX属性，因为属性直接累加）
    // 注意：现在只有当前属性，没有基础属性

    // 等级属性（仅记录升级次数，不与属性挂钩）
    int level;              // 飞船等级（仅记录踩过区域的次数）
    // 移除 levelMultiplier 成员变量

    // 升级冷却系统（新增）
    float upgradeCooldown;  // 升级冷却计时器，为0时可以触发升级

    // 等级显示字体（静态共享）
    static sf::Font levelFont;
    static bool levelFontLoaded;

public:
    Spaceship(Vector2D pos, sf::Color col, int faction, IPhysicsSystem* physSys, float m = 3.0f, float h = 100.0f);

    void update(float deltaTime) override;

    // 应用推进力（朝船头方向）
    void applyThrust(float force);

    // 应用制动力（移动方向的反方向推力，即阻力）
    void applyBrake(float intensity = 1.0f);

    // 获取精确碰撞像素块 - 为三角形生成近似像素表示
    std::vector<std::shared_ptr<PixelBlock>> getPixelBlocks() const override;

    // 获取凸包顶点（就是三角形的三个顶点）
    std::vector<Vector2D> getConvexHull() const override;

    // 获取外接圆半径
    float getBoundingRadius() const override;

    // 获取对象类型标识
    std::string getType() const override { return "Spaceship"; }

    // 尝试攻击目标
    bool tryAttack(std::shared_ptr<PhysicsObject> targetObj);

    void draw(sf::RenderWindow& window) override;

    // 绘制建造环
    void drawBuildRing(sf::RenderWindow& window);

    // 开始建造新飞船
    void startBuilding();

    // 检查是否正在建造
    bool isCurrentlyBuilding() const { return isBuilding; }

    // 获取建造进度
    float getBuildProgress() const { return buildProgress; }

    // 获取飞船基础颜色（公共访问方法）
    sf::Color getBaseColor() const { return baseColor; }

    // 获取视野范围
    float getViewRange() const { return viewRange; }

    // 获取攻击范围
    float getFireRange() const { return fireRange; }

    // 获取攻击角度
    float getFireAngle() const { return fireAngle; }

    // 飞船爆炸效果
    std::vector<std::shared_ptr<PhysicsObject>> onDestroy() override;

    // 生成光点
    std::vector<std::shared_ptr<LightPoint>> generateLightPoints() override;

    // 限制飞船在窗口内
    bool isOutOfBounds() override;

    // 重置建造状态（公共函数供PhysicsSystem访问）
    void resetBuilding() {
        isBuilding = false;
        buildProgress = 0;
        buildTimer = 0;
    }

    // 获取收集距离
    float getCollectionDistance() const { return collectionDistance; }

    // AI相关函数
    void updateAI(float deltaTime);
    void evaluateState();
    void avoidingBehavior(float deltaTime);
    void attackingBehavior(float deltaTime);
    void collectingBehavior(float deltaTime);
    void miningBehavior(float deltaTime);
    void idleBehavior(float deltaTime);
    void turningBehavior(float deltaTime);

    // 目标查找函数
    std::shared_ptr<PhysicsObject> findObstacleInPath();
    std::shared_ptr<PhysicsObject> findEnemyInView();
    std::shared_ptr<LightPoint> findLightPointInView(); // 查找视野内的光点
    std::shared_ptr<PhysicsObject> findAsteroidInView(); // 改为视野内而非攻击范围内

    // 收集附近光点
    void collectNearbyLightPoints();

    // 应用升级效果
    void applyUpgrade(int upgradeType);

    // 获取采矿专精
    float getMiningExpertise() const { return miningExpertise; }

    // 检查是否在窗口内可见
    bool isInWindowView() const;

    // 移除 updateFromLevel 方法，因为等级不再影响属性

    // 获取和设置等级
    int getLevel() const { return level; }
    void setLevel(int lvl) { level = lvl; }

    // 复制属性
    void copyAttributesFrom(const std::shared_ptr<Spaceship>& other) {
        // 复制等级
        this->level = other->level;

        // 直接复制所有当前属性值（后代继承所有已升级的属性）
        this->maneuverability = other->maneuverability;
        this->maxSpeed = other->maxSpeed;
        this->accelerationRate = other->accelerationRate;
        this->brakeRate = other->brakeRate;
        this->fireRate = other->fireRate;
        this->fireRange = other->fireRange;
        this->damage = other->damage;
        this->fireAngle = other->fireAngle;
        this->recoveryRate = other->recoveryRate;
        this->miningExpertise = other->miningExpertise;
        this->viewRange = other->viewRange;
        this->collectionDistance = other->collectionDistance;
        this->buildSpeed = other->buildSpeed;
        this->maxHealth = other->maxHealth;  // 也要复制最大生命值
        this->health = this->maxHealth;      // 后代满血
        this->upgradeCooldown = 0;           // 重置升级冷却

        // 复制颜色
        this->baseColor = other->baseColor;
        this->glowColor = other->glowColor;

        this->updateAppearance(); // 确保外观正确更新
    }

    // 获取视野圆圈形状
    sf::CircleShape getVisionCircle() const {
        sf::CircleShape circle;
        circle.setRadius(viewRange);
        circle.setOrigin(viewRange, viewRange);
        circle.setPosition(position.toSFML());
        return circle;
    }

    // 获取AI状态字符串
    std::string getAIStateString() const {
        switch (currentState) {
        case AIState::ATTACKING: return "攻击中";
        case AIState::COLLECTING: return "采集中";
        case AIState::MINING: return "采矿中";
        case AIState::AVOIDING: return "避障中";
        case AIState::IDLE: return "空闲中";
        case AIState::TURNING: return "转向中";
        default: return "未知";
        }
    }

    // 获取所有属性字符串
    std::string getAttributesString() const {
        std::stringstream ss;
        ss << "等级: " << level << " \n"
            << "生命: " << static_cast<int>(health) << "/" << static_cast<int>(maxHealth) << "\n"
            << "机动性: " << std::fixed << std::setprecision(1) << maneuverability << "\n"
            << "最大速度: " << static_cast<int>(maxSpeed) << "\n"
            << "射速: " << std::fixed << std::setprecision(1) << fireRate << "\n"
            << "伤害: " << static_cast<int>(damage) << "\n"
            << "射程: " << static_cast<int>(fireRange) << "\n"
            << "视野: " << static_cast<int>(viewRange) << "\n"
            << "恢复力: " << std::fixed << std::setprecision(1) << recoveryRate << "\n"
            << "采矿专精: " << std::fixed << std::setprecision(2) << miningExpertise << "\n"
            << "建造速度: " << std::fixed << std::setprecision(2) << buildSpeed << "\n"
            << "升级冷却: " << std::fixed << std::setprecision(1) << upgradeCooldown << "秒\n"
            << "行为模式: " << getAIStateString();
        return ss.str();
    }

    // 设置选中状态
    void setSelected(bool selected) {
        isSelected = selected;
    }

    bool getIsSelected() const {
        return isSelected;
    }

    void setBaseColor(sf::Color newColor) {
        baseColor = newColor;
        glowColor = newColor;
        updateAppearance(); // 更新飞船外观
    }

    // 检查是否可升级（新增）
    bool canUpgrade() const { return upgradeCooldown <= 0; }

    // 设置升级冷却（新增）
    void setUpgradeCooldown(float cooldown) { upgradeCooldown = cooldown; }

    // 获取升级冷却（新增）
    float getUpgradeCooldown() const { return upgradeCooldown; }

private:
    bool isSelected = false; // 是否被选中

    // 检查点是否在三角形内（重心坐标法）
    bool pointInTriangle(Vector2D p, Vector2D a, Vector2D b, Vector2D c) const;

    void updateAppearance();

    // 检查前方是否有障碍物或窗口边界（新增）
    bool checkFrontObstacle(float checkDistance);

    // 安全的物理系统访问
    bool hasPhysicsSystem() const { return physicsSystem != nullptr; }
};

// 初始化静态字体成员
sf::Font Spaceship::levelFont;
bool Spaceship::levelFontLoaded = false;

// ====================== 小行星类（像素方块组成） ======================
class Asteroid : public PhysicsObject {
public:
    // 小行星类型
    enum class AsteroidType {
        NORMAL,      // 普通小行星
        FAST_TRAIL,  // 极速小行星，有拖尾特效
        GRAVITY      // 引力小行星
    };
private:
    std::vector<std::shared_ptr<PixelBlock>> pixelBlocks;
    sf::Color baseColor;
    float pixelSize;
    bool canSplit;

    // 用于碰撞检测的凸包顶点
    mutable std::vector<Vector2D> cachedConvexHull;
    mutable bool hullDirty;

    // 物理系统接口引用（用于生成光点）
    IPhysicsSystem* physicsSystem;

    AsteroidType asteroidType;

    // 圆形拖尾结构
    struct CircleTrail {
        Vector2D position;   // 圆的位置（生成时的位置）
        float currentRadius; // 当前半径
        float maxRadius;     // 最大半径
        float lifetime;      // 当前生存时间
        float maxLifetime;   // 最大生存时间（1.5秒）
    };

    std::vector<CircleTrail> circleTrails; // 圆形拖尾列表
    float trailTimer;                      // 拖尾生成计时器
    float trailUpdateInterval;             // 拖尾生成间隔

    // 彗星头部效果
    sf::CircleShape cometHead;
    sf::CircleShape cometAura;            // 新增：不透明但看起来像透明的大型气场
    float cometPulseTime;

    // 引力相关（仅用于GRAVITY类型）
    float gravityRadius;  // 引力作用半径
    float gravityStrength; // 引力强度

    // 渲染优化：顶点数组
    mutable sf::VertexArray vertexArray;
    mutable bool verticesDirty;

    // 新增：基础质量和血量（用于三维模拟）
    float baseMass;      // 基础质量（基于体积）
    float baseHealth;    // 基础血量（基于横截面积）
    int totalPixelCount; // 总像素数

    // 新增：碰撞缓冲系统
    float cushionFactor;      // 缓冲系数（0.0-1.0），减少碰撞伤害
    float impactAbsorption;   // 冲击吸收能力，基于质量比
    float lastCollisionTime;  // 上次碰撞时间，用于防止连续碰撞

public:

    Asteroid(Vector2D pos, Vector2D vel, float radius, float m = 50.0f, float h = 200.0f, int faction = -1, IPhysicsSystem* physSys = nullptr, AsteroidType type = AsteroidType::NORMAL)
        : PhysicsObject(pos, m, h, faction), pixelSize(4.0f), canSplit(true), hullDirty(true), physicsSystem(physSys), asteroidType(type),
        vertexArray(sf::Quads), verticesDirty(true), totalPixelCount(0), cometPulseTime(0),
        cushionFactor(0.0f), impactAbsorption(0.0f), lastCollisionTime(0.0f) {

        velocity = vel;
        setRestitution(0.1f);

        // 根据小行星类型设置属性
        switch (asteroidType) {
        case AsteroidType::NORMAL:
            // 普通小行星颜色（灰色系）
            generateNormalColor();
            generatePixelAsteroid(radius);
            cushionFactor = 0.3f;  // 普通小行星有30%减伤
            break;
        case AsteroidType::FAST_TRAIL:
        {
            // 极速小行星（彗星白）
            baseColor = sf::Color(220, 230, 255);  // 彗星白，带一点蓝
            generatePixelAsteroid(radius);

            // 初始化圆形拖尾系统
            trailTimer = 0;
            trailUpdateInterval = 0.04f;  // 更频繁地生成拖尾（25次/秒）
            circleTrails.clear();         // 清空拖尾列表

            // 初始化彗星头部和底气场
            float headRadius = getBoundingRadius() * 1.0f;
            cometHead.setRadius(headRadius);
            cometHead.setFillColor(sf::Color(180, 200, 255));  // 彗星头部颜色
            cometHead.setOrigin(headRadius, headRadius);
            cometHead.setPosition(position.toSFML());

            // 初始化气场（更大的圆形）
            float auraRadius = headRadius * 2.5f;
            cometAura.setRadius(auraRadius);
            cometAura.setFillColor(sf::Color(80, 100, 142));
            cometAura.setOrigin(auraRadius, auraRadius);
            cometAura.setPosition(position.toSFML());

            cushionFactor = 0.6f;  // 极速小行星有60%减伤，非常坚固
        }
        break;
        case AsteroidType::GRAVITY:
            // 引力小行星（暗红色）
            baseColor = sf::Color(150, 50, 50);
            generatePixelAsteroid(radius);
            gravityRadius = radius * 1.5f;  // 引力半径是小行星半径的1.5倍
            gravityStrength = baseMass * 0.5f;      // 引力强度
            cushionFactor = 0.4f;  // 引力小行星有40%减伤
            break;
        }

        // 计算总像素数
        totalPixelCount = static_cast<int>(pixelBlocks.size());

        // 增强坚固性：三维模拟
        // 质量与体积（radius³）成正比
        baseMass = radius * radius * radius * 0.1f;  // 体积系数

        // 血量与质量^(2/3)成正比，模拟三维物体血量与横截面积的关系
        // 血量 = k * 质量^(2/3)
        baseHealth = 100.0f * std::pow(baseMass, 2.0f / 3.0f);

        // 特殊小行星获得额外坚固系数
        if (asteroidType == AsteroidType::FAST_TRAIL) {
            baseMass *= 3.0f;      // 极速小行星质量3倍（更重）
            baseHealth *= 5.0f;    // 极速小行星血量5倍（更坚固）
        }
        else if (asteroidType == AsteroidType::GRAVITY) {
            baseMass *= 8.0f;      // 引力小行星质量8倍
            baseHealth *= 10.0f;   // 引力小行星血量10倍（非常坚固）
        }

        // 更新质量和血量
        updateMassAndHealthFromPixels();

        // 计算冲击吸收能力：基于质量和尺寸
        impactAbsorption = std::log10(baseMass) * 0.5f;
    }

    // 简化构造函数，方便std::make_shared使用
    Asteroid(Vector2D pos, Vector2D vel, float radius, float m, float h, int faction)
        : Asteroid(pos, vel, radius, m, h, faction, nullptr, AsteroidType::NORMAL) {
    }

    ~Asteroid() {
        // 清理批处理数据
        g_renderBatchSystem.removeBatch(this);
    }

    void update(float deltaTime) override {
        PhysicsObject::update(deltaTime);

        // 更新上次碰撞时间
        if (lastCollisionTime > 0) {
            lastCollisionTime -= deltaTime;
        }

        // 根据类型处理特殊逻辑
        switch (asteroidType) {
        case AsteroidType::FAST_TRAIL:
            updateTrail(deltaTime);
            break;
        case AsteroidType::GRAVITY:
            applyGravity(deltaTime);
            break;
        }

        // 标记顶点数据需要更新
        verticesDirty = true;
    }

    // 重写takeDamage方法，应用缓冲系统
    void takeDamage(float damage, Vector2D damagePoint = Vector2D(0, 0), std::shared_ptr<PhysicsObject> attacker = nullptr) override {
        if (!isActive) return;

        // 应用缓冲系数减伤
        float reducedDamage = damage * (1.0f - cushionFactor);

        // 如果上次碰撞时间很短，进一步减伤（防止连续碰撞造成过大伤害）
        if (lastCollisionTime > 0) {
            reducedDamage *= 0.5f;  // 连续碰撞伤害减半
        }

        // 更新碰撞时间
        lastCollisionTime = 0.5f;

        // 应用伤害
        health -= reducedDamage;

        // 确保血量不为负
        if (health <= 0) {
            health = 0;
            isActive = false;
            return;
        }

        // 如果伤害点接近原点，使用随机边缘点
        if ((damagePoint - position).length() < 1.0f) {
            float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
            damagePoint = position + Vector2D(std::cos(angle), std::sin(angle)) * getBoundingRadius();
        }

        // 造成边缘损坏
        causeEdgeDamage(damagePoint, reducedDamage, attacker);

        // 更新质量和血量
        updateMassAndHealthFromPixels();

        // 标记凸包需要更新
        hullDirty = true;
        verticesDirty = true;
    }

    // 获取所有像素块
    std::vector<std::shared_ptr<PixelBlock>> getPixelBlocks() const override {
        std::vector<std::shared_ptr<PixelBlock>> activeBlocks;
        for (const auto& pixel : pixelBlocks) {
            if (pixel->isActive()) {
                activeBlocks.push_back(pixel);
            }
        }
        return activeBlocks;
    }

    // 获取凸包顶点（使用Graham Scan算法）
    std::vector<Vector2D> getConvexHull() const override {
        if (!hullDirty && !cachedConvexHull.empty()) {
            return cachedConvexHull;
        }

        std::vector<Vector2D> points;

        // 收集所有活跃像素块的中心点
        for (const auto& pixel : pixelBlocks) {
            if (pixel->isActive()) {
                points.push_back(pixel->getWorldPosition(position, rotation));
            }
        }

        if (points.size() < 3) {
            // 如果少于3个点，返回最小包围盒
            if (points.empty()) {
                cachedConvexHull = { position };
            }
            else if (points.size() == 1) {
                cachedConvexHull = { points[0] };
            }
            else {
                // 两个点的情况
                cachedConvexHull = { points[0], points[1] };
            }
            hullDirty = false;
            return cachedConvexHull;
        }

        // Graham Scan算法
        // 1. 找到最左下角的点
        int n = static_cast<int>(points.size());
        int minIdx = 0;
        for (int i = 1; i < n; i++) {
            if (points[i].y < points[minIdx].y ||
                (points[i].y == points[minIdx].y && points[i].x < points[minIdx].x)) {
                minIdx = i;
            }
        }

        // 交换到第一个位置
        std::swap(points[0], points[minIdx]);

        // 2. 按极角排序
        Vector2D p0 = points[0];
        std::sort(points.begin() + 1, points.end(),
            [&p0](const Vector2D& a, const Vector2D& b) {
                Vector2D v1 = a - p0;
                Vector2D v2 = b - p0;
                float cross = v1.cross(v2);
                if (std::abs(cross) < 1e-6) {
                    return v1.lengthSquared() < v2.lengthSquared();
                }
                return cross > 0;
            });

        // 3. 构建凸包
        std::vector<Vector2D> hull;
        hull.push_back(points[0]);
        hull.push_back(points[1]);

        for (int i = 2; i < n; i++) {
            while (hull.size() >= 2) {
                Vector2D& p2 = hull[hull.size() - 2];
                Vector2D& p1 = hull[hull.size() - 1];
                Vector2D p = points[i];

                Vector2D v1 = p1 - p2;
                Vector2D v2 = p - p1;

                if (v1.cross(v2) <= 0) {
                    hull.pop_back();
                }
                else {
                    break;
                }
            }
            hull.push_back(points[i]);
        }

        cachedConvexHull = hull;
        hullDirty = false;
        return hull;
    }

    // 获取外接圆半径
    float getBoundingRadius() const override {
        if (pixelBlocks.empty()) return 0;

        float maxDist = 0;
        for (const auto& pixel : pixelBlocks) {
            if (pixel->isActive()) {
                float dist = pixel->getDistance();
                if (dist > maxDist) maxDist = dist;
            }
        }
        return maxDist + pixelSize * 0.7f;
    }

    void draw(sf::RenderWindow& window) override {
        if (isActive) {
            // 只在禁生成区内绘制
            if (isInNoGenZone()) {
                // 绘制拖尾特效（如果是极速小行星）
                if (asteroidType == AsteroidType::FAST_TRAIL) {
                    drawTrail(window);
                }

                // 使用批处理系统绘制小行星
                if (verticesDirty) {
                    updateVertexArray();
                }
                g_renderBatchSystem.drawAsteroidBatch(this, window);
            }
        }
    }

    // 更新顶点数组
    void updateVertexArray() const {
        auto activeBlocks = getPixelBlocks();
        g_renderBatchSystem.updateAsteroidBatch(this, activeBlocks, position, rotation);
        verticesDirty = false;
    }

    // 获取对象类型标识
    std::string getType() const override { return "Asteroid"; }

    // 检查是否可以被采集（小行星不可被采集，只有光点可以被采集）
    bool isCollectible() const override { return false; }

    // 获取像素数量
    int getPixelCount() const override {
        return countActivePixels();
    }

    // 小行星破碎 - 重写以遵循动量守恒
    std::vector<std::shared_ptr<PhysicsObject>> onDestroy() override {
        std::vector<std::shared_ptr<PhysicsObject>> fragments;

        if (pixelBlocks.size() <= 1) {
            // 只有一个像素块，不分裂
            return fragments;
        }

        // 根据剩余像素块数量决定分裂成几块
        int activePixels = countActivePixels();
        if (activePixels == 0) return fragments;

        // 分裂成2-4块，基于剩余像素数
        int fragmentCount = std::min(4, std::max(2, activePixels / 15));

        // 将像素块分组
        std::vector<std::vector<std::shared_ptr<PixelBlock>>> pixelGroups(fragmentCount);

        // 使用K-means简单分组
        std::vector<Vector2D> centers(fragmentCount);
        for (int i = 0; i < fragmentCount; i++) {
            float angle = (2.0f * PI * i) / fragmentCount;
            centers[i] = Vector2D(std::cos(angle), std::sin(angle)) * (getBoundingRadius() * 0.5f);
        }

        // 分配像素块到最近的组
        for (const auto& pixel : pixelBlocks) {
            if (!pixel->isActive()) continue;

            Vector2D pixelPos = pixel->getLocalPosition();
            int closestGroup = 0;
            float minDist = (pixelPos - centers[0]).lengthSquared();

            for (int i = 1; i < fragmentCount; i++) {
                float dist = (pixelPos - centers[i]).lengthSquared();
                if (dist < minDist) {
                    minDist = dist;
                    closestGroup = i;
                }
            }

            pixelGroups[closestGroup].push_back(pixel);
        }

        // 计算总动量
        Vector2D totalMomentum = velocity * mass;

        // 存储碎片质量和速度
        std::vector<float> fragmentMasses;
        std::vector<Vector2D> fragmentBaseVelocities;
        std::vector<Vector2D> fragmentRandomVelocities;

        Vector2D randomMomentumSum(0, 0);
        float totalFragmentMass = 0.0f;

        // 预计算碎片信息
        for (int i = 0; i < fragmentCount; i++) {
            if (pixelGroups[i].empty()) continue;

            // 计算碎片中心
            Vector2D fragmentCenter(0, 0);
            for (const auto& pixel : pixelGroups[i]) {
                fragmentCenter = fragmentCenter + pixel->getLocalPosition();
            }
            fragmentCenter = fragmentCenter / static_cast<float>(pixelGroups[i].size());

            // 计算碎片位置（相对于原小行星）
            Vector2D fragmentWorldPos = position + fragmentCenter.rotated(rotation);

            // 计算碎片质量
            float activeRatio = static_cast<float>(pixelGroups[i].size()) / static_cast<float>(totalPixelCount);
            float fragmentMass = baseMass * activeRatio;
            fragmentMasses.push_back(fragmentMass);
            totalFragmentMass += fragmentMass;

            // 基础速度（基于动量守恒）
            Vector2D baseVelocity = velocity; // 继承原速度

            // 随机爆炸速度（方向随机，大小基于碎片质量）
            float explosionStrength = 5.0f * (1.0f / std::sqrt(fragmentMass + 1.0f));
            float randomAngle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
            Vector2D randomVelocity = Vector2D(std::cos(randomAngle), std::sin(randomAngle)) * explosionStrength;

            fragmentBaseVelocities.push_back(baseVelocity);
            fragmentRandomVelocities.push_back(randomVelocity);
            randomMomentumSum = randomMomentumSum + randomVelocity * fragmentMass;
        }

        // 调整随机速度以确保总动量守恒
        if (totalFragmentMass > 0) {
            Vector2D randomMomentumCorrection = randomMomentumSum / totalFragmentMass;

            // 创建碎片
            int fragmentIndex = 0;
            for (int i = 0; i < fragmentCount; i++) {
                if (pixelGroups[i].empty()) continue;

                float fragmentMass = fragmentMasses[fragmentIndex];
                Vector2D baseVelocity = fragmentBaseVelocities[fragmentIndex];
                Vector2D randomVelocity = fragmentRandomVelocities[fragmentIndex];

                // 修正随机速度以确保动量守恒
                Vector2D correctedRandomVelocity = randomVelocity - randomMomentumCorrection;

                // 最终速度 = 基础速度 + 修正后的随机速度
                Vector2D fragmentVel = baseVelocity + correctedRandomVelocity;

                fragmentIndex++;

                // 计算碎片中心（再次计算）
                Vector2D fragmentCenter(0, 0);
                for (const auto& pixel : pixelGroups[i]) {
                    fragmentCenter = fragmentCenter + pixel->getLocalPosition();
                }
                fragmentCenter = fragmentCenter / static_cast<float>(pixelGroups[i].size());

                // 计算碎片位置（相对于原小行星）
                Vector2D fragmentWorldPos = position + fragmentCenter.rotated(rotation);

                // 创建新小行星（碎片都是普通类型）
                float fragmentRadius = calculateGroupRadius(pixelGroups[i], fragmentCenter);

                // 增强坚固性：使用新的质量血量计算公式
                float fragmentBaseMass = fragmentRadius * fragmentRadius * fragmentRadius * 0.1f;
                float fragmentBaseHealth = 100.0f * std::pow(fragmentBaseMass, 2.0f / 3.0f);

                // 按比例分配血量
                float activeRatio = static_cast<float>(pixelGroups[i].size()) / static_cast<float>(totalPixelCount);
                float fragmentHealth = baseHealth * activeRatio;

                auto fragment = std::make_shared<Asteroid>(fragmentWorldPos, fragmentVel, fragmentRadius,
                    fragmentMass, fragmentHealth, -1, physicsSystem, AsteroidType::NORMAL);

                // 设置碎片像素块
                fragment->pixelBlocks.clear();
                for (const auto& pixel : pixelGroups[i]) {
                    Vector2D localPos = pixel->getLocalPosition() - fragmentCenter;
                    fragment->pixelBlocks.push_back(std::make_shared<PixelBlock>(
                        localPos, pixelSize, pixel->getColor()));
                }

                // 设置碎片的总像素数
                fragment->totalPixelCount = static_cast<int>(pixelGroups[i].size());
                fragment->baseMass = fragmentBaseMass;
                fragment->baseHealth = fragmentBaseHealth;

                // 设置旋转（随机）
                fragment->setRotation(rotation + static_cast<float>(rand() % 100) / 100.0f * PI);
                fragment->setAngularVelocity(angularVelocity + static_cast<float>(rand() % 100) / 100.0f * 2.0f - 1.0f);

                fragments.push_back(fragment);
            }
        }

        return fragments;
    }

    // 生成光点
    std::vector<std::shared_ptr<LightPoint>> generateLightPoints() override {
        std::vector<std::shared_ptr<LightPoint>> lightPoints;

        // 小行星被摧毁时生成0-1个光点
        int lightPointCount = rand() % 2; // 0到1个

        for (int i = 0; i < lightPointCount; i++) {
            // 随机位置（在小行星表面）
            float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
            float distance = getBoundingRadius() * 0.8f;
            Vector2D offset = Vector2D(std::cos(angle), std::sin(angle)) * distance;

            // 随机速度（爆炸效果）
            Vector2D vel = velocity + offset.normalized() * (1.0f + static_cast<float>(rand() % 100) / 100.0f);

            // 光点颜色（灰色或随机颜色）
            sf::Color lightColor;
            if (rand() % 100 < 70) {
                // 70%概率为灰色
                int gray = 150 + rand() % 50;
                lightColor = sf::Color(gray, gray, gray);
            }
            else {
                // 30%概率为随机彩色
                lightColor = sf::Color(rand() % 200 + 55, rand() % 200 + 55, rand() % 200 + 55);
            }

            auto lightPoint = std::make_shared<LightPoint>(position + offset, vel, lightColor);
            lightPoints.push_back(lightPoint);
        }

        return lightPoints;
    }

    // 设置物理系统引用
    void setPhysicsSystem(IPhysicsSystem* physSys) { physicsSystem = physSys; }

    // 获取小行星类型
    AsteroidType getAsteroidType() const { return asteroidType; }

    // 获取缓冲系数（用于物理系统碰撞计算）
    float getCushionFactor() const { return cushionFactor; }

private:
    void generateNormalColor() {
        int grayValue = 120 + rand() % 60;
        int colorVar = rand() % 40 - 20;
        baseColor = sf::Color(
            std::clamp(grayValue + colorVar, 80, 200),
            std::clamp(grayValue + colorVar, 80, 200),
            std::clamp(grayValue + colorVar, 80, 200)
        );
    }

    void generatePixelAsteroid(float radius) {
        // 确保半径至少为像素大小
        radius = std::max(radius, pixelSize);

        int gridRadius = static_cast<int>(radius / pixelSize);

        // 确保gridRadius至少为1
        gridRadius = std::max(1, gridRadius);

        // 使用多种算法组合生成不规则形状
        std::vector<std::vector<bool>> grid(gridRadius * 2 + 1, std::vector<bool>(gridRadius * 2 + 1, false));

        // 生成噪声形状
        for (int x = -gridRadius; x <= gridRadius; x++) {
            for (int y = -gridRadius; y <= gridRadius; y++) {
                float distance = std::sqrt(static_cast<float>(x * x + y * y));
                float normalizedDist = distance / static_cast<float>(gridRadius);

                if (normalizedDist > 1.0f) continue;

                // 使用柏林噪声的简化版本
                float noise = 0.5f + 0.5f * std::sin(static_cast<float>(x) * 0.3f) * std::cos(static_cast<float>(y) * 0.3f);
                float threshold = 0.7f + 0.3f * noise;

                // 边缘衰减
                float edgeFactor = 1.0f - normalizedDist * normalizedDist;
                threshold *= edgeFactor;

                if (normalizedDist < threshold) {
                    int gridX = x + gridRadius;
                    int gridY = y + gridRadius;
                    grid[gridX][gridY] = true;
                }
            }
        }

        // 腐蚀和膨胀操作使形状更自然
        applyMorphologicalOperations(grid, 1);

        // 创建像素块
        for (size_t x = 0; x < grid.size(); x++) {
            for (size_t y = 0; y < grid[0].size(); y++) {
                if (grid[x][y]) {
                    int worldX = static_cast<int>(x) - gridRadius;
                    int worldY = static_cast<int>(y) - gridRadius;

                    // 随机颜色变化
                    int colorVar = rand() % 30 - 15;
                    sf::Color pixelColor = baseColor;
                    pixelColor.r = static_cast<sf::Uint8>(std::clamp(static_cast<int>(pixelColor.r) + colorVar, 80, 200));
                    pixelColor.g = static_cast<sf::Uint8>(std::clamp(static_cast<int>(pixelColor.g) + colorVar, 80, 200));
                    pixelColor.b = static_cast<sf::Uint8>(std::clamp(static_cast<int>(pixelColor.b) + colorVar, 80, 200));

                    Vector2D localPos(static_cast<float>(worldX) * pixelSize, static_cast<float>(worldY) * pixelSize);
                    pixelBlocks.push_back(std::make_shared<PixelBlock>(localPos, pixelSize, pixelColor));
                }
            }
        }

        // 确保有足够像素块
        int maxAttempts = 10;
        int attempts = 0;

        // 如果像素块太少，调整参数重新生成
        while (pixelBlocks.size() < 5 && attempts < maxAttempts) {
            attempts++;

            // 清除现有像素块
            pixelBlocks.clear();

            // 调整阈值重新生成
            gridRadius = std::max(1, gridRadius + attempts); // 逐渐增大网格

            // 重新分配网格
            grid = std::vector<std::vector<bool>>(gridRadius * 2 + 1, std::vector<bool>(gridRadius * 2 + 1, false));

            // 使用更宽松的阈值重新生成
            for (int x = -gridRadius; x <= gridRadius; x++) {
                for (int y = -gridRadius; y <= gridRadius; y++) {
                    float distance = std::sqrt(static_cast<float>(x * x + y * y));
                    float normalizedDist = distance / static_cast<float>(gridRadius);

                    if (normalizedDist > 1.0f) continue;

                    // 使用更宽松的阈值
                    float noise = 0.6f + 0.4f * std::sin(static_cast<float>(x) * 0.2f) * std::cos(static_cast<float>(y) * 0.2f);
                    float threshold = 0.8f + 0.2f * noise; // 更宽松的阈值

                    // 边缘衰减更少
                    float edgeFactor = 1.0f - normalizedDist * 0.5f; // 减少衰减
                    threshold *= edgeFactor;

                    if (normalizedDist < threshold) {
                        int gridX = x + gridRadius;
                        int gridY = y + gridRadius;
                        grid[gridX][gridY] = true;
                    }
                }
            }

            // 创建像素块
            for (size_t x = 0; x < grid.size(); x++) {
                for (size_t y = 0; y < grid[0].size(); y++) {
                    if (grid[x][y]) {
                        int worldX = static_cast<int>(x) - gridRadius;
                        int worldY = static_cast<int>(y) - gridRadius;

                        // 随机颜色变化
                        int colorVar = rand() % 30 - 15;
                        sf::Color pixelColor = baseColor;
                        pixelColor.r = static_cast<sf::Uint8>(std::clamp(static_cast<int>(pixelColor.r) + colorVar, 80, 200));
                        pixelColor.g = static_cast<sf::Uint8>(std::clamp(static_cast<int>(pixelColor.g) + colorVar, 80, 200));
                        pixelColor.b = static_cast<sf::Uint8>(std::clamp(static_cast<int>(pixelColor.b) + colorVar, 80, 200));

                        Vector2D localPos(static_cast<float>(worldX) * pixelSize, static_cast<float>(worldY) * pixelSize);
                        pixelBlocks.push_back(std::make_shared<PixelBlock>(localPos, pixelSize, pixelColor));
                    }
                }
            }
        }

        // 如果尝试多次后仍然没有足够的像素块，创建一个简单的小行星
        if (pixelBlocks.size() < 3) {
            pixelBlocks.clear();
            // 创建一个简单的小行星（3x3网格）
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue; // 跳过中心

                    Vector2D localPos(static_cast<float>(x) * pixelSize, static_cast<float>(y) * pixelSize);
                    pixelBlocks.push_back(std::make_shared<PixelBlock>(localPos, pixelSize, baseColor));
                }
            }
        }

        // 记录总像素数
        totalPixelCount = static_cast<int>(pixelBlocks.size());
    }

    void applyMorphologicalOperations(std::vector<std::vector<bool>>& grid, int iterations) {
        int width = static_cast<int>(grid.size());
        int height = static_cast<int>(grid[0].size());

        for (int iter = 0; iter < iterations; iter++) {
            std::vector<std::vector<bool>> newGrid = grid;

            for (int x = 1; x < width - 1; x++) {
                for (int y = 1; y < height - 1; y++) {
                    if (!grid[x][y]) {
                        // 腐蚀：如果周围大多数是空的，则移除
                        int emptyNeighbors = 0;
                        for (int dx = -1; dx <= 1; dx++) {
                            for (int dy = -1; dy <= 1; dy++) {
                                if (!grid[x + dx][y + dy]) emptyNeighbors++;
                            }
                        }

                        if (emptyNeighbors > 6) {
                            newGrid[x][y] = false;
                        }
                    }
                    else {
                        // 膨胀：如果周围大多数是实的，则填充
                        int solidNeighbors = 0;
                        for (int dx = -1; dx <= 1; dx++) {
                            for (int dy = -1; dy <= 1; dy++) {
                                if (grid[x + dx][y + dy]) solidNeighbors++;
                            }
                        }

                        if (solidNeighbors > 4) {
                            newGrid[x][y] = true;
                        }
                    }
                }
            }

            grid = newGrid;
        }
    }

    void causeEdgeDamage(Vector2D damagePoint, float damage, std::shared_ptr<PhysicsObject> attacker) {
        // 将伤害点转换到本地坐标系
        Vector2D localDamagePoint = (damagePoint - position).rotated(-rotation);

        // 计算要移除的像素块数量
        // 增强坚固性：伤害效率降低，更大的小行星更难被破坏
        float damageEfficiency = 1.0f / std::sqrt(getBoundingRadius()); // 半径越大，伤害效率越低
        int pixelsToRemove = static_cast<int>(damage / 10.0f * damageEfficiency);
        pixelsToRemove = std::max(1, std::min(pixelsToRemove, countActivePixels() / 4)); // 最多移除1/4的像素

        // 按距离排序像素块（从伤害点最近的开始）
        std::vector<std::shared_ptr<PixelBlock>> sortedPixels;
        for (const auto& pixel : pixelBlocks) {
            if (!pixel->isActive()) continue;

            sortedPixels.push_back(pixel);
        }

        std::sort(sortedPixels.begin(), sortedPixels.end(),
            [&localDamagePoint](const std::shared_ptr<PixelBlock>& a, const std::shared_ptr<PixelBlock>& b) {
                float distA = (a->getLocalPosition() - localDamagePoint).lengthSquared();
                float distB = (b->getLocalPosition() - localDamagePoint).lengthSquared();
                return distA < distB;
            });

        // 移除最近的像素块，并检查是否生成光点
        for (int i = 0; i < std::min(pixelsToRemove, static_cast<int>(sortedPixels.size())); i++) {
            sortedPixels[i]->setActive(false);

            // 每个被移除的像素有0.05的概率产生一个光点
            // 如果攻击者是飞船，加上其采矿专精
            float lightPointProbability = 0.05f;

            if (attacker && attacker->getType() == "Spaceship") {
                auto spaceship = std::dynamic_pointer_cast<Spaceship>(attacker);
                if (spaceship) {
                    lightPointProbability += spaceship->getMiningExpertise();
                }
            }

            if (static_cast<float>(rand() % 100) / 100.0f < lightPointProbability) {
                // 生成光点
                Vector2D pixelWorldPos = sortedPixels[i]->getWorldPosition(position, rotation);
                Vector2D lightPointVel = velocity + (pixelWorldPos - position).normalized() * (1.0f + static_cast<float>(rand() % 100) / 100.0f);

                // 光点颜色使用像素颜色
                sf::Color lightColor = sortedPixels[i]->getColor();

                auto lightPoint = std::make_shared<LightPoint>(pixelWorldPos, lightPointVel, lightColor);

                // 添加到物理系统
                if (physicsSystem) {
                    physicsSystem->addLightPoint(lightPoint);
                }
            }
        }

        verticesDirty = true;
    }

    void updateMassAndHealthFromPixels() {
        int activeCount = countActivePixels();
        if (activeCount == 0) {
            isActive = false;
            return;
        }

        // 增强坚固性：基于基础质量和血量计算当前值
        float activeRatio = static_cast<float>(activeCount) / static_cast<float>(totalPixelCount);

        // 当前质量 = 基础质量 × 活跃像素比例
        mass = baseMass * activeRatio;

        // 当前血量 = 基础血量 × 活跃像素比例
        health = baseHealth * activeRatio;

        // 确保最大血量也更新
        maxHealth = baseHealth * activeRatio;
    }

    int countActivePixels() const {
        int count = 0;
        for (const auto& pixel : pixelBlocks) {
            if (pixel->isActive()) count++;
        }
        return count;
    }

    float calculateGroupRadius(const std::vector<std::shared_ptr<PixelBlock>>& group,
        const Vector2D& center) const {
        float maxDist = 0;
        for (const auto& pixel : group) {
            float dist = (pixel->getLocalPosition() - center).length();
            if (dist > maxDist) maxDist = dist;
        }
        return maxDist + pixelSize;
    }

    // 更新拖尾特效 - 圆形拖尾版本
    void updateTrail(float deltaTime) {
        // 更新彗星头部脉动效果（轻微脉动）
        cometPulseTime += deltaTime * 1.5f; // 减慢脉动速度
        if (cometPulseTime > 2.0f * PI) cometPulseTime -= 2.0f * PI;

        // 更新头部位置
        cometHead.setPosition(position.toSFML());

        // 更新气动位置（保持和彗星头部一起移动）
        cometAura.setPosition(position.toSFML());

        // 更新现有圆形拖尾
        for (auto it = circleTrails.begin(); it != circleTrails.end();) {
            it->lifetime += deltaTime;

            // 计算缩小比例（0到1）
            float shrinkRatio = 1.0f - (it->lifetime / it->maxLifetime);

            // 圆形逐渐缩小（从最大半径缩小到0）
            it->currentRadius = it->maxRadius * shrinkRatio;

            // 如果圆形缩小到很小或时间到，删除它
            if (it->lifetime >= it->maxLifetime || it->currentRadius < 1.0f) {
                it = circleTrails.erase(it);
            }
            else {
                ++it;
            }
        }

        // 定期添加新的圆形拖尾
        trailTimer += deltaTime;
        if (trailTimer >= trailUpdateInterval) {
            trailTimer = 0;

            // 计算速度影响因子
            float speed = velocity.length();
            float speedFactor = std::min(speed / 100.0f, 2.0f);

            // 创建新的圆形拖尾
            CircleTrail newTrail;
            newTrail.position = position; // 记录当前彗星位置

            // 起始半径：基于彗星大小和速度，确保足够大
            newTrail.maxRadius = getBoundingRadius() * (1.01f + speedFactor * 0.01f);
            newTrail.currentRadius = newTrail.maxRadius;

            // 生存时间：恒定
            newTrail.maxLifetime = 1.5f;
            newTrail.lifetime = 0.0f;

            circleTrails.push_back(newTrail);

            // 限制拖尾数量，避免过多
            if (circleTrails.size() > 50) {
                circleTrails.erase(circleTrails.begin());
            }
        }
    }

    // 绘制拖尾特效 - 圆形拖尾版本
    void drawTrail(sf::RenderWindow& window) {
        if (circleTrails.empty()) return;

        // 绘制彗星头部（几乎不脉动）
        float pulse = 0.99f + 0.01f * std::sin(cometPulseTime); // 非常轻微的脉动
        float speedRatio = std::min(velocity.length() / 150.0f, 1.0f);
        float currentRadius = getBoundingRadius();

        // 彗星头部（较小的实心圆）
        float headRadius = currentRadius * 1.0f;
        cometHead.setRadius(headRadius);
        cometHead.setOrigin(headRadius, headRadius);

        // 头部颜色（速度越快越亮，不透明）
        int headIntensity = 180 + static_cast<int>(75 * speedRatio);
        sf::Color headColor = sf::Color(headIntensity, headIntensity, 255); // 纯蓝色，不透明
        cometHead.setFillColor(headColor);
        cometHead.setOutlineColor(sf::Color(255, 255, 255, 50));
        cometHead.setOutlineThickness(1.0f);

        // 绘制彗星气场（大型不透明圆，但看起来像透明）
        float auraRadius = headRadius * 1.05f;
        cometAura.setRadius(auraRadius);
        cometAura.setOrigin(auraRadius, auraRadius);

        // 气动颜色：更淡的蓝色，完全填充
        sf::Color auraColor = sf::Color(120, 140, 255);
        cometAura.setFillColor(auraColor);
        cometAura.setOutlineColor(sf::Color::Transparent);

        // 先绘制气场
        window.draw(cometAura);

        // 绘制所有圆形拖尾（从新到旧绘制，这样旧的在下面）
        for (auto it = circleTrails.rbegin(); it != circleTrails.rend(); ++it) {
            const auto& trail = *it;

            // 计算拖尾的"年龄比例"（0到1）
            float ageRatio = trail.lifetime / trail.maxLifetime;

            // 拖尾越老，颜色越淡（但始终保持不透明）
            float intensity = 1.0f - (ageRatio * 0.8f); // 从100%到20%强度
            int colorIntensity = static_cast<int>(255 * intensity);

            // 创建拖尾圆形
            sf::CircleShape trailCircle(trail.currentRadius);
            trailCircle.setOrigin(trail.currentRadius, trail.currentRadius);
            trailCircle.setPosition(trail.position.toSFML());

            // 拖尾颜色：根据年龄渐变，从不透明蓝到不透明浅蓝
            // 越老的拖尾颜色越浅，尺寸越小
            sf::Color trailColor = sf::Color(
                static_cast<sf::Uint8>(100 * intensity),  // R
                static_cast<sf::Uint8>(120 * intensity),  // G
                static_cast<sf::Uint8>(255 * intensity)   // B
            );
            trailCircle.setFillColor(trailColor);
            trailCircle.setOutlineColor(sf::Color::Transparent);

            window.draw(trailCircle);

            // 在拖尾中心添加一个更亮的小点
            if (trail.currentRadius > 10.0f) {
                float centerRadius = trail.currentRadius * 0.3f;
                sf::CircleShape centerCircle(centerRadius);
                centerCircle.setOrigin(centerRadius, centerRadius);
                centerCircle.setPosition(trail.position.toSFML());
                centerCircle.setFillColor(sf::Color(200, 220, 255));
                window.draw(centerCircle);
            }
        }

        // 最后绘制彗星头部（在最上面）
        window.draw(cometHead);

        // 在最外层添加一个轻微的发光效果（不透明但很淡）
        sf::CircleShape outerGlow(headRadius * 1.1f);
        outerGlow.setOrigin(outerGlow.getRadius(), outerGlow.getRadius());
        outerGlow.setPosition(position.toSFML());
        outerGlow.setFillColor(sf::Color(80, 100, 200));
        window.draw(outerGlow);

        // 添加随机星光粒子效果（可选）
        if (speedRatio > 0.5f && rand() % 100 < 8) {
            // 在拖尾末端添加随机星光
            if (!circleTrails.empty()) {
                const auto& newestTrail = circleTrails.back();

                for (int i = 0; i < 3; i++) {
                    // 随机偏移
                    float angle = static_cast<float>(rand() % 360) * PI / 180.0f;
                    float distance = static_cast<float>(rand() % 20);

                    Vector2D offset(std::cos(angle) * distance, std::sin(angle) * distance);
                    Vector2D starPos = newestTrail.position + offset;

                    sf::CircleShape star(2.0f + static_cast<float>(rand() % 4));
                    star.setFillColor(sf::Color(255, 255, 255));
                    star.setOrigin(star.getRadius(), star.getRadius());
                    star.setPosition(starPos.toSFML());
                    window.draw(star);
                }
            }
        }
    }

    // 应用引力（仅用于GRAVITY类型）
    void applyGravity(float deltaTime) {
        if (!physicsSystem) return;

        // 获取所有对象
        auto allObjects = physicsSystem->getAllObjects();

        for (const auto& obj : allObjects) {
            if (!obj->getIsActive()) continue;
            if (obj.get() == this) continue; // 不对自己施加引力

            // 计算距离
            Vector2D toObj = obj->getPosition() - position;
            float distance = toObj.length();

            // 如果在引力半径内
            if (distance < gravityRadius && distance > 0.1f) {
                // 计算引力方向
                Vector2D gravityDir = toObj.normalized();

                // 计算引力大小（与距离平方成反比）
                float gravityForce = gravityStrength / (distance * distance);

                // 对对象施加引力
                Vector2D gravityAcceleration = gravityDir * gravityForce;
                obj->applyForce(gravityAcceleration * obj->getMass());
            }
        }
    }
};

// ====================== 精确碰撞检测系统 ======================
class PhysicsSystem : public IPhysicsSystem {
private:
    std::vector<std::shared_ptr<PhysicsObject>> objects;
    std::vector<std::shared_ptr<LightPoint>> lightPoints; // 光点列表
    std::vector<std::shared_ptr<PhysicsObject>> objectsToAdd;
    std::vector<std::shared_ptr<LightPoint>> lightPointsToAdd;

    // 升级区域列表（包括增援和五种升级区域）
    std::vector<std::shared_ptr<UpgradeZone>> upgradeZones;
    std::vector<std::shared_ptr<UpgradeZone>> zonesToAdd;

    // 激光列表
    std::vector<LaserBeam> lasers;

    // 空间分区网格
    static const int GRID_SIZE = 100;
    static const int GRID_WIDTH = 36;  // 3600/100 = 36（扩大后）
    static const int GRID_HEIGHT = 27; // 2700/100 ≈ 27（扩大后）
    std::vector<std::vector<std::vector<std::shared_ptr<PhysicsObject>>>> spatialGrid;

    // 阵营资源统计（按阵营ID索引）
    static const int MAX_FACTIONS = 4;
    std::array<float, MAX_FACTIONS> factionResources;
    std::array<int, MAX_FACTIONS> factionShipCounts;

    // 升级区域生成计时器
    float upgradeZoneTimer;
    float nextZoneTime; // 下一个区域生成时间（6-30秒随机）

    // 增援区域复制计时器
    std::map<std::shared_ptr<UpgradeZone>, float> reinforcementTimers;

    // 精确像素碰撞检测
    bool checkPixelCollision(std::shared_ptr<PhysicsObject> a, std::shared_ptr<PhysicsObject> b) {
        // 如果对象是光点，跳过碰撞检测
        if (a->getType() == "LightPoint" || b->getType() == "LightPoint") {
            return false;
        }

        // 优化：如果任一对象不在禁生成区内，不进行碰撞检测
        if (!a->isInNoGenZone() || !b->isInNoGenZone()) {
            return false;
        }

        auto pixelsA = a->getPixelBlocks();
        auto pixelsB = b->getPixelBlocks();

        for (const auto& pixelA : pixelsA) {
            if (!pixelA->isActive()) continue;

            for (const auto& pixelB : pixelsB) {
                if (!pixelB->isActive()) continue;

                // 检查两个像素块是否碰撞
                if (checkPixelPixelCollision(pixelA, a->getPosition(), a->getRotation(),
                    pixelB, b->getPosition(), b->getRotation())) {
                    return true;
                }
            }
        }

        return false;
    }

    bool checkPixelPixelCollision(std::shared_ptr<PixelBlock> pixelA, Vector2D posA, float rotA,
        std::shared_ptr<PixelBlock> pixelB, Vector2D posB, float rotB) {
        // 获取世界坐标下的包围盒
        sf::FloatRect rectA = pixelA->getWorldBounds(posA, rotA);
        sf::FloatRect rectB = pixelB->getWorldBounds(posB, rotB);

        // 快速AABB检测
        if (!rectA.intersects(rectB)) return false;

        // 精确检测：检查像素块A的四个角是否在像素块B内，反之亦然
        std::vector<Vector2D> cornersA = {
            Vector2D(rectA.left, rectA.top),
            Vector2D(rectA.left + rectA.width, rectA.top),
            Vector2D(rectA.left + rectA.width, rectA.top + rectA.height),
            Vector2D(rectA.left, rectA.top + rectA.height)
        };

        for (const auto& corner : cornersA) {
            if (pixelB->contains(corner, posB, rotB)) return true;
        }

        std::vector<Vector2D> cornersB = {
            Vector2D(rectB.left, rectB.top),
            Vector2D(rectB.left + rectB.width, rectB.top),
            Vector2D(rectB.left + rectB.width, rectB.top + rectB.height),
            Vector2D(rectB.left, rectB.top + rectB.height)
        };

        for (const auto& corner : cornersB) {
            if (pixelA->contains(corner, posA, rotA)) return true;
        }

        return false;
    }

    // 激光与物体碰撞检测
    std::shared_ptr<PhysicsObject> checkLaserCollision(LaserBeam& laser, Vector2D& collisionPoint) {
        Vector2D start = laser.getStart();
        Vector2D end = laser.getEnd();
        std::shared_ptr<PhysicsObject> hitObject = nullptr;
        float minDist = std::numeric_limits<float>::max();
        auto owner = laser.getOwner();

        // 激光终点光球半径（增加命中判定范围）
        float hitRadius = 12.0f;

        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            if (owner && obj.get() == owner.get()) continue; // 不能击中自己
            if (obj->getType() == "LightPoint") continue; // 光点不能被激光击中

            // 优化：只检测禁生成区内的物体
            if (!obj->isInNoGenZone()) continue;

            // 首先检查激光终点光球是否与物体相交
            float distToEnd = (obj->getPosition() - end).length();
            if (distToEnd <= hitRadius + obj->getBoundingRadius()) {
                // 光球命中，使用最近的点
                Vector2D toObj = obj->getPosition() - end;
                Vector2D hitPoint = end + toObj.normalized() * (hitRadius + obj->getBoundingRadius() * 0.5f);
                float dist = (hitPoint - start).lengthSquared();
                if (dist < minDist) {
                    minDist = dist;
                    collisionPoint = hitPoint;
                    hitObject = obj;

                    // 如果是小行星，调整激光终点到小行星表面
                    if (obj->getType() == "Asteroid") {
                        // 计算从小行星中心指向激光起点的方向
                        Vector2D toStart = start - obj->getPosition();
                        Vector2D surfacePoint = obj->getPosition() + toStart.normalized() * obj->getBoundingRadius();
                        laser.setEndPos(surfacePoint);
                        collisionPoint = surfacePoint;
                    }

                    continue; // 光球命中优先
                }
            }

            // 原有的线段与凸包相交检测
            auto hull = obj->getConvexHull();
            if (hull.size() < 2) continue;

            for (size_t i = 0; i < hull.size(); i++) {
                Vector2D p1 = hull[i];
                Vector2D p2 = hull[(i + 1) % hull.size()];

                Vector2D intersect = lineSegmentIntersection(start, end, p1, p2);
                if (intersect.x != -1) {
                    float dist = (intersect - start).lengthSquared();
                    if (dist < minDist) {
                        minDist = dist;
                        collisionPoint = intersect;
                        hitObject = obj;

                        // 如果是小行星，调整激光终点到碰撞点
                        if (obj->getType() == "Asteroid") {
                            laser.setEndPos(intersect);
                        }
                    }
                }
            }

            // 如果找到碰撞，立即返回（激光只能击中第一个物体）
            if (hitObject) break;
        }

        return hitObject;
    }

    Vector2D lineSegmentIntersection(Vector2D a1, Vector2D a2, Vector2D b1, Vector2D b2) {
        Vector2D s1 = a2 - a1;
        Vector2D s2 = b2 - b1;

        float s, t;
        float denom = -s2.x * s1.y + s1.x * s2.y;

        if (std::abs(denom) < 1e-6) {
            return Vector2D(-1, -1); // 平行或共线
        }

        s = (-s1.y * (a1.x - b1.x) + s1.x * (a1.y - b1.y)) / denom;
        t = (s2.x * (a1.y - b1.y) - s2.y * (a1.x - b1.x)) / denom;

        if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
            return Vector2D(a1.x + (t * s1.x), a1.y + (t * s1.y));
        }

        return Vector2D(-1, -1); // 没有交点
    }

    // 计算碰撞响应 - 修改：考虑小行星缓冲系统
    void resolveCollision(std::shared_ptr<PhysicsObject> a, std::shared_ptr<PhysicsObject> b,
        Vector2D collisionPoint, Vector2D normal) {
        // 如果对象是光点，跳过碰撞响应
        if (a->getType() == "LightPoint" || b->getType() == "LightPoint") {
            return;
        }

        // 计算相对速度
        Vector2D relativeVel = b->getVelocity() - a->getVelocity();
        float velAlongNormal = relativeVel.dot(normal);

        // 如果物体正在分离，不处理
        if (velAlongNormal > 0) return;

        // 计算恢复系数
        float restitution = std::min(a->getRestitution(), b->getRestitution());

        // 计算冲量
        float impulseMag = -(1.0f + restitution) * velAlongNormal;
        impulseMag /= (1.0f / a->getMass() + 1.0f / b->getMass());

        Vector2D impulse = normal * impulseMag;

        // 应用冲量
        a->addVelocity(-impulse / a->getMass());
        b->addVelocity(impulse / b->getMass());

        // 计算动能（考虑缓冲系统）
        float kineticEnergy = 0.5f * a->getMass() * a->getVelocity().lengthSquared() +
            0.5f * b->getMass() * b->getVelocity().lengthSquared();

        // 计算质量比，减少大物体撞小物体的伤害
        float massRatio = 1.0f;
        if (a->getType() == "Asteroid" && b->getType() == "Asteroid") {
            // 两个小行星碰撞，质量大的对小的影响小
            float largerMass = std::max(a->getMass(), b->getMass());
            float smallerMass = std::min(a->getMass(), b->getMass());
            if (largerMass > smallerMass * 10.0f) {
                // 质量相差10倍以上，大幅减少伤害
                massRatio = smallerMass / largerMass;
            }
        }

        // 计算伤害（考虑缓冲和弹性损耗）
        float damage = kineticEnergy * 0.002f * massRatio;

        // 如果是小行星，应用缓冲系数
        if (a->getType() == "Asteroid") {
            auto asteroidA = std::dynamic_pointer_cast<Asteroid>(a);
            if (asteroidA) {
                // 小行星之间的碰撞伤害进一步减少
                if (b->getType() == "Asteroid") {
                    damage *= 0.3f; // 小行星间碰撞伤害减少70%
                }
            }
        }
        if (b->getType() == "Asteroid") {
            auto asteroidB = std::dynamic_pointer_cast<Asteroid>(b);
            if (asteroidB) {
                // 小行星之间的碰撞伤害进一步减少
                if (a->getType() == "Asteroid") {
                    damage *= 0.3f; // 小行星间碰撞伤害减少70%
                }
            }
        }

        // 应用伤害
        a->takeDamage(damage, collisionPoint, b);
        b->takeDamage(damage, collisionPoint, a);
    }

    // 更新空间分区网格（优化：只将禁生成区内的物体放入网格）
    void updateSpatialGrid() {
        // 清空网格
        for (auto& row : spatialGrid) {
            for (auto& cell : row) {
                cell.clear();
            }
        }

        // 将物体放入网格（排除光点）
        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;

            // 光点不参与碰撞检测，不加入空间网格
            if (obj->getType() == "LightPoint") {
                continue;
            }

            // 优化：只将禁生成区内的物体放入网格
            if (!obj->isInNoGenZone()) {
                continue;
            }

            Vector2D pos = obj->getPosition();
            float radius = obj->getBoundingRadius();

            // 计算物体所在的网格范围
            int minX = std::max(0, static_cast<int>((pos.x - radius) / GRID_SIZE));
            int maxX = std::min(GRID_WIDTH - 1, static_cast<int>((pos.x + radius) / GRID_SIZE));
            int minY = std::max(0, static_cast<int>((pos.y - radius) / GRID_SIZE));
            int maxY = std::min(GRID_HEIGHT - 1, static_cast<int>((pos.y + radius) / GRID_SIZE));

            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    spatialGrid[y][x].push_back(obj);
                }
            }
        }
    }

    // 获取物体周围的候选碰撞对象（优化：只返回禁生成区内的物体）
    std::vector<std::shared_ptr<PhysicsObject>> getNearbyObjects(std::shared_ptr<PhysicsObject> obj) {
        std::vector<std::shared_ptr<PhysicsObject>> nearby;
        Vector2D pos = obj->getPosition();

        int gridX = std::min(GRID_WIDTH - 1, std::max(0, static_cast<int>(pos.x / GRID_SIZE)));
        int gridY = std::min(GRID_HEIGHT - 1, std::max(0, static_cast<int>(pos.y / GRID_SIZE)));

        // 检查3x3区域
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int x = gridX + dx;
                int y = gridY + dy;

                if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
                    for (const auto& nearbyObj : spatialGrid[y][x]) {
                        if (nearbyObj != obj && nearbyObj->getIsActive()) {
                            nearby.push_back(nearbyObj);
                        }
                    }
                }
            }
        }

        return nearby;
    }

    // 处理激光碰撞
    void processLasers(float deltaTime) {
        // 更新激光
        for (auto it = lasers.begin(); it != lasers.end();) {
            it->update(deltaTime);

            if (!it->isAlive()) {
                it = lasers.erase(it);
                continue;
            }

            // 如果激光已经击中过目标，跳过碰撞检测
            if (it->getHasHit()) {
                ++it;
                continue;
            }

            // 检查激光碰撞
            Vector2D collisionPoint;
            auto hitObject = checkLaserCollision(*it, collisionPoint);

            if (hitObject) {
                // 对击中的物体造成伤害
                hitObject->takeDamage(it->getDamage(), collisionPoint, it->getOwner());

                // 标记激光已经击中
                it->setHasHit(true);

                // 激光击中后不会立即消失，会继续显示直到生命周期结束
                ++it;
            }
            else {
                ++it;
            }
        }
    }

    // 处理升级区域
    void processUpgradeZones(float deltaTime) {
        // 更新升级区域生成计时器
        upgradeZoneTimer += deltaTime;

        // 每nextZoneTime秒生成一个升级区域
        if (upgradeZoneTimer >= nextZoneTime) {
            // 在窗口区域内随机生成位置
            float x = 100.0f + static_cast<float>(rand() % (WINDOW_WIDTH - 200));
            float y = 100.0f + static_cast<float>(rand() % (WINDOW_HEIGHT - 200));

            // 随机选择区域类型（0-5，共6种）
            int zoneType = rand() % 6;
            std::shared_ptr<UpgradeZone> zone;

            switch (zoneType) {
            case 0: // 增援区域
                zone = std::make_shared<ReinforcementZone>(Vector2D(x, y));
                break;
            case 1: // 生存升级
                zone = std::make_shared<SurvivalUpgradeZone>(Vector2D(x, y));
                break;
            case 2: // 进攻升级
                zone = std::make_shared<AttackUpgradeZone>(Vector2D(x, y));
                break;
            case 3: // 机动升级
                zone = std::make_shared<MobilityUpgradeZone>(Vector2D(x, y));
                break;
            case 4: // 生产升级
                zone = std::make_shared<ProductionUpgradeZone>(Vector2D(x, y));
                break;
            case 5: // 领域升级
                zone = std::make_shared<DomainUpgradeZone>(Vector2D(x, y));
                break;
            }

            zonesToAdd.push_back(zone);

            // 重置计时器并设置下一个区域生成时间（6-30秒随机）
            upgradeZoneTimer = 0;
            nextZoneTime = 6.0f + static_cast<float>(rand() % 25); // 6-30秒
        }

        // 添加新区域
        upgradeZones.insert(upgradeZones.end(), zonesToAdd.begin(), zonesToAdd.end());
        zonesToAdd.clear();

        // 更新区域
        for (auto it = upgradeZones.begin(); it != upgradeZones.end();) {
            (*it)->update(deltaTime);

            if (!(*it)->isActive()) {
                reinforcementTimers.erase(*it);
                it = upgradeZones.erase(it);
            }
            else {
                // 处理区域效果
                processZoneEffects(*it, deltaTime);
                ++it;
            }
        }
    }

    // 处理区域效果（修改：添加升级冷却检查）
    void processZoneEffects(std::shared_ptr<UpgradeZone> zone, float deltaTime) {
        // 增援区域特殊处理
        if (zone->getZoneType() == 0) {
            // 更新增援计时器
            reinforcementTimers[zone] += deltaTime;

            // 每3秒复制一艘飞船
            if (reinforcementTimers[zone] >= 3.0f) {
                // 获取区域内的所有飞船
                std::vector<std::shared_ptr<Spaceship>> shipsInZone;

                for (const auto& obj : objects) {
                    if (!obj->getIsActive()) continue;
                    if (obj->getType() == "Spaceship" && zone->contains(obj->getPosition())) {
                        auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                        if (ship) {
                            shipsInZone.push_back(ship);
                        }
                    }
                }

                if (!shipsInZone.empty()) {
                    // 随机选择一艘飞船复制
                    int randomIndex = rand() % shipsInZone.size();
                    auto shipToCopy = shipsInZone[randomIndex];

                    // 在增援区域内复制飞船
                    reinforceShipInZone(zone, shipToCopy);
                }

                reinforcementTimers[zone] = 0;
            }
        }
        // 其他升级区域
        else {
            // 检查区域内的飞船
            for (const auto& obj : objects) {
                if (!obj->getIsActive()) continue;
                if (obj->getType() == "Spaceship") {
                    if (zone->contains(obj->getPosition())) {
                        auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                        if (ship && ship->canUpgrade()) {
                            // 应用区域效果
                            ship->applyUpgrade(zone->getZoneType());
                            // 设置5秒升级冷却
                            ship->setUpgradeCooldown(5.0f);
                        }
                    }
                }
            }
        }
    }

    // 在增援区域内复制飞船
    void reinforceShipInZone(std::shared_ptr<UpgradeZone> zone, std::shared_ptr<Spaceship> shipToCopy) {
        // 复制飞船（直接创建新飞船，不需要建造时间）
        Vector2D newPos = zone->getRandomPosition();
        sf::Color color = shipToCopy->getBaseColor();
        int faction = shipToCopy->getFactionId();

        auto newShip = std::make_shared<Spaceship>(newPos, color, faction, this,
            shipToCopy->getMass(), shipToCopy->getMaxHealth());

        // 复制所有属性
        newShip->copyAttributesFrom(shipToCopy);

        // 设置随机方向和速度
        float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
        newShip->setRotation(angle);

        float speed = 20.0f + static_cast<float>(rand() % 100) / 100.0f * 30.0f;
        Vector2D velocity(std::cos(angle), std::sin(angle));
        newShip->setVelocity(velocity * speed);

        objectsToAdd.push_back(newShip);
    }

    // 处理光点收集 - 简化版本：检查飞船附近的收集距离
    void processLightPointCollection() {
        // 检查所有飞船，收集附近的光点
        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            if (obj->getType() == "Spaceship") {
                auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                if (ship) {
                    ship->collectNearbyLightPoints();
                }
            }
        }

        // 移除已收集的光点（不活跃的光点）
        lightPoints.erase(std::remove_if(lightPoints.begin(), lightPoints.end(),
            [](const std::shared_ptr<LightPoint>& lp) {
                return !lp->getIsActive();
            }), lightPoints.end());
    }

    // 处理飞船建造
    void processShipBuilding() {
        for (int faction = 0; faction < MAX_FACTIONS; faction++) {
            // 检查是否有足够资源建造新飞船
            if (factionResources[faction] >= RESOURCE_TO_BUILD) {
                // 查找该阵营中等级最高的飞船（且不在建造中）
                std::shared_ptr<Spaceship> bestShip = nullptr;
                int highestLevel = 0;

                for (const auto& obj : objects) {
                    if (!obj->getIsActive()) continue;
                    if (obj->getType() == "Spaceship" && obj->getFactionId() == faction) {
                        auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                        if (ship && !ship->isCurrentlyBuilding()) {
                            int shipLevel = ship->getLevel();
                            if (shipLevel > highestLevel) {
                                highestLevel = shipLevel;
                                bestShip = ship;
                            }
                            // 如果等级相同，选择生命值百分比更高的
                            else if (shipLevel == highestLevel && bestShip) {
                                float currentHealthRatio = ship->getHealth() / ship->getMaxHealth();
                                float bestHealthRatio = bestShip->getHealth() / bestShip->getMaxHealth();
                                if (currentHealthRatio > bestHealthRatio) {
                                    bestShip = ship;
                                }
                            }
                        }
                    }
                }

                // 如果找到合适的飞船，开始建造
                if (bestShip && !bestShip->isCurrentlyBuilding()) {
                    bestShip->startBuilding();
                    factionResources[faction] -= RESOURCE_TO_BUILD;
                }
            }

            // 检查是否有飞船完成建造
            for (const auto& obj : objects) {
                if (!obj->getIsActive()) continue;
                if (obj->getType() == "Spaceship" && obj->getFactionId() == faction) {
                    auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                    if (ship && ship->isCurrentlyBuilding() && ship->getBuildProgress() >= 1.0f) {
                        // 创建新飞船
                        createNewShip(ship);
                        // 重置建造状态
                        ship->resetBuilding();
                    }
                }
            }
        }
    }

    // 创建新飞船（复制所有属性）
    void createNewShip(std::shared_ptr<Spaceship> builder) {
        // 获取建造者信息
        Vector2D pos = builder->getPosition();
        sf::Color color = builder->getBaseColor();
        int faction = builder->getFactionId();

        // 创建新飞船（复制建造者的所有属性，血量为满血）
        auto newShip = std::make_shared<Spaceship>(pos, color, faction, this,
            builder->getMass(), builder->getMaxHealth());

        // 复制所有属性
        newShip->copyAttributesFrom(builder);

        // 设置满血
        newShip->setHealth(newShip->getMaxHealth());

        // 复制其他属性
        newShip->setRotation(builder->getRotation());
        newShip->setAngularVelocity(builder->getAngularVelocity());
        newShip->setVelocity(builder->getVelocity());
        newShip->setRestitution(builder->getRestitution());

        // 轻微偏移以避免完全重叠
        float offset = 30.0f;
        float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
        Vector2D offsetVec = Vector2D(std::cos(angle), std::sin(angle)) * offset;
        newShip->setPosition(pos + offsetVec);

        // 朝向窗口中心
        Vector2D center(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
        Vector2D toCenter = center - newShip->getPosition();
        newShip->setRotation(toCenter.angle());

        // 初始速度（继承建造者速度加上随机偏移）
        Vector2D initialVel = builder->getVelocity() + offsetVec.normalized() * 5.0f;
        newShip->setVelocity(initialVel);

        // 添加到系统
        objectsToAdd.push_back(newShip);
    }

    void cleanupOutOfWindowObjects() {
        // 清理 objects 列表中的物体
        for (auto it = objects.begin(); it != objects.end();) {
            auto& obj = *it;

            if (!obj->getIsActive()) {
                ++it;
                continue;
            }

            bool isOutOfWindow = !obj->isInWindow();

            // 对于光点，只要在窗口外就立即删除（保持不变）
            if (isOutOfWindow && obj->getType() == "LightPoint") {
                it = objects.erase(it);
                continue;
            }

            // 对于小行星，只有窗口外且像素数小于8才立即删除
            if (isOutOfWindow && obj->getType() == "Asteroid") {
                auto asteroid = std::dynamic_pointer_cast<Asteroid>(obj);
                if (asteroid) {
                    // 获取小行星的像素数量
                    int pixelCount = asteroid->getPixelCount();
                    if (pixelCount < 8) {
                        // 像素数小于8，立即删除
                        it = objects.erase(it);
                        continue;
                    }
                    // 像素数大于等于8，保留（按原有逻辑处理）
                }
            }

            // 对于其他物体（飞船等），如果在窗口外且静止，则删除（原有逻辑）
            if (obj->isOutOfWindowAndStatic()) {
                it = objects.erase(it);
                continue;
            }

            ++it;
        }

        // 同时清理光点列表中的光点（窗口外立即删除）
        lightPoints.erase(std::remove_if(lightPoints.begin(), lightPoints.end(),
            [](const std::shared_ptr<LightPoint>& lp) {
                return !lp->getIsActive() || !lp->isInWindow();
            }), lightPoints.end());
    }


public:
    PhysicsSystem() : upgradeZoneTimer(0), nextZoneTime(6.0f) {
        spatialGrid.resize(GRID_HEIGHT, std::vector<std::vector<std::shared_ptr<PhysicsObject>>>(GRID_WIDTH));
        factionResources.fill(0.0f);
        factionShipCounts.fill(0);
    }

    // IPhysicsSystem 接口实现
    void addLightPoint(std::shared_ptr<LightPoint> lightPoint) override {
        lightPointsToAdd.push_back(lightPoint);
        // 光点也是物理对象，添加到objects列表
        objectsToAdd.push_back(lightPoint);
    }

    const std::vector<std::shared_ptr<LightPoint>>& getLightPoints() const override {
        return lightPoints;
    }

    std::vector<std::shared_ptr<LightPoint>> getAllLightPoints() const override {
        return lightPoints;
    }

    void addLaser(const LaserBeam& laser) override {
        lasers.push_back(laser);
    }

    float getFactionResource(int faction) const override {
        if (faction >= 0 && faction < MAX_FACTIONS) {
            return factionResources[faction];
        }
        return 0.0f;
    }

    void setFactionResource(int faction, float amount) override {
        if (faction >= 0 && faction < MAX_FACTIONS) {
            factionResources[faction] = amount;
        }
    }

    std::vector<std::shared_ptr<PhysicsObject>> getObjectsInWindow() const override {
        std::vector<std::shared_ptr<PhysicsObject>> windowObjects;

        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;

            // 检查对象是否在窗口内
            Vector2D pos = obj->getPosition();
            if (pos.x >= 0 && pos.x <= WINDOW_WIDTH &&
                pos.y >= 0 && pos.y <= WINDOW_HEIGHT) {
                windowObjects.push_back(obj);
            }
        }

        return windowObjects;
    }

    std::vector<std::shared_ptr<PhysicsObject>> getAllObjects() const override {
        return objects;
    }

    std::vector<std::shared_ptr<Spaceship>> getAllSpaceships() const override {
        std::vector<std::shared_ptr<Spaceship>> ships;
        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            if (obj->getType() == "Spaceship") {
                auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                if (ship) {
                    ships.push_back(ship);
                }
            }
        }
        return ships;
    }

    bool isObjectInWindow(const std::shared_ptr<PhysicsObject>& obj) const override {
        if (!obj) return false;
        Vector2D pos = obj->getPosition();
        return pos.x >= 0 && pos.x <= WINDOW_WIDTH &&
            pos.y >= 0 && pos.y <= WINDOW_HEIGHT;
    }

    // 原有公共接口
    void addObject(std::shared_ptr<PhysicsObject> obj) {
        objectsToAdd.push_back(obj);

        // 如果是小行星，设置物理系统引用
        if (obj->getType() == "Asteroid") {
            auto asteroid = std::dynamic_pointer_cast<Asteroid>(obj);
            if (asteroid) {
                asteroid->setPhysicsSystem(this);
            }
        }
    }

    void update(float deltaTime) {
        // 添加新物体
        objects.insert(objects.end(), objectsToAdd.begin(), objectsToAdd.end());
        objectsToAdd.clear();

        // 添加新光点
        lightPoints.insert(lightPoints.end(), lightPointsToAdd.begin(), lightPointsToAdd.end());
        lightPointsToAdd.clear();

        // 统一清理屏幕外物体
        cleanupOutOfWindowObjects();

        // 移除飞出事件边界的物体
        objects.erase(std::remove_if(objects.begin(), objects.end(),
            [](const std::shared_ptr<PhysicsObject>& obj) {
                if (!obj->getIsActive()) return true;
                return obj->isOutOfBounds();
            }),
            objects.end());

        // 更新所有物体（包括禁生成区外的物体）
        for (auto& obj : objects) {
            if (obj->getIsActive()) {
                obj->update(deltaTime);

                // 更新飞船的升级冷却
                if (obj->getType() == "Spaceship") {
                    auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                    if (ship && ship->getUpgradeCooldown() > 0) {
                        ship->setUpgradeCooldown(ship->getUpgradeCooldown() - deltaTime);
                    }
                }
            }
        }

        // 更新空间分区（只包含禁生成区内的物体）
        updateSpatialGrid();

        // 处理激光
        processLasers(deltaTime);

        // 处理升级区域
        processUpgradeZones(deltaTime);

        // 处理光点收集 - 简化版本
        processLightPointCollection();

        // 处理飞船建造
        processShipBuilding();

        // 碰撞检测与响应（只检测禁生成区内的物体）
        std::vector<std::pair<std::shared_ptr<PhysicsObject>, std::shared_ptr<PhysicsObject>>> collisions;

        for (size_t i = 0; i < objects.size(); i++) {
            if (!objects[i]->getIsActive()) continue;

            // 跳过光点
            if (objects[i]->getType() == "LightPoint") continue;

            // 优化：只检测禁生成区内的物体
            if (!objects[i]->isInNoGenZone()) continue;

            auto nearby = getNearbyObjects(objects[i]);

            for (auto& obj2 : nearby) {
                if (obj2 == objects[i] || !obj2->getIsActive()) continue;

                // 跳过光点
                if (obj2->getType() == "LightPoint") continue;

                // 检查是否已经检测过这对碰撞
                bool alreadyChecked = false;
                for (const auto& pair : collisions) {
                    if ((pair.first == objects[i] && pair.second == obj2) ||
                        (pair.first == obj2 && pair.second == objects[i])) {
                        alreadyChecked = true;
                        break;
                    }
                }

                if (alreadyChecked) continue;

                // 包围圆快速检测
                float dist = (objects[i]->getPosition() - obj2->getPosition()).length();
                float minDist = objects[i]->getBoundingRadius() + obj2->getBoundingRadius();

                if (dist < minDist) {
                    // 精确像素碰撞检测
                    if (checkPixelCollision(objects[i], obj2)) {
                        collisions.emplace_back(objects[i], obj2);
                    }
                }
            }
        }

        // 处理碰撞
        for (const auto& collision : collisions) {
            auto& a = collision.first;
            auto& b = collision.second;

            if (!a->getIsActive() || !b->getIsActive()) continue;

            // 计算碰撞法线（从a指向b）
            Vector2D normal = (b->getPosition() - a->getPosition()).normalized();
            Vector2D collisionPoint = (a->getPosition() + b->getPosition()) * 0.5f;

            resolveCollision(a, b, collisionPoint, normal);
        }

        // 处理物体销毁和碎片生成
        for (auto it = objects.begin(); it != objects.end();) {
            if (!(*it)->getIsActive()) {
                // 生成碎片
                auto fragments = (*it)->onDestroy();
                objectsToAdd.insert(objectsToAdd.end(), fragments.begin(), fragments.end());

                // 生成光点
                auto newLightPoints = (*it)->generateLightPoints();
                for (auto& lp : newLightPoints) {
                    addLightPoint(lp);
                }

                it = objects.erase(it);
            }
            else {
                ++it;
            }
        }

        // 更新飞船数量统计（必须在所有物体更新后）
        std::fill(factionShipCounts.begin(), factionShipCounts.end(), 0);
        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            if (obj->getType() == "Spaceship") {
                int faction = obj->getFactionId();
                if (faction >= 0 && faction < MAX_FACTIONS) {
                    factionShipCounts[faction]++;
                }
            }
        }
    }

    void draw(sf::RenderWindow& window) {
        // 绘制光点（优化：只绘制禁生成区内的光点）
        for (auto& lp : lightPoints) {
            if (lp->isInNoGenZone()) {
                lp->draw(window);
            }
        }

        // 绘制激光
        for (auto& laser : lasers) {
            // 优化：如果激光起点和终点都不在禁生成区内，不绘制
            if (laser.getStart().x < -NO_GEN_ZONE_MARGIN || laser.getStart().x > WINDOW_WIDTH + NO_GEN_ZONE_MARGIN ||
                laser.getStart().y < -NO_GEN_ZONE_MARGIN || laser.getStart().y > WINDOW_HEIGHT + NO_GEN_ZONE_MARGIN) {
                if (laser.getEnd().x < -NO_GEN_ZONE_MARGIN || laser.getEnd().x > WINDOW_WIDTH + NO_GEN_ZONE_MARGIN ||
                    laser.getEnd().y < -NO_GEN_ZONE_MARGIN || laser.getEnd().y > WINDOW_HEIGHT + NO_GEN_ZONE_MARGIN) {
                    continue;
                }
            }
            laser.draw(window);
        }

        // 绘制升级区域
        for (auto& zone : upgradeZones) {
            zone->draw(window);
        }

        // 绘制物体（优化：只绘制禁生成区内的物体）
        for (auto& obj : objects) {
            if (obj->getIsActive() && obj->isInNoGenZone()) {
                obj->draw(window);
            }
        }
    }

    void clear() {
        objects.clear();
        lightPoints.clear();
        objectsToAdd.clear();
        lightPointsToAdd.clear();
        lasers.clear();
        upgradeZones.clear();
        zonesToAdd.clear();
        factionResources.fill(0.0f);
        factionShipCounts.fill(0);
        upgradeZoneTimer = 0;
        nextZoneTime = 6.0f;
        reinforcementTimers.clear();
        g_renderBatchSystem.clear();
    }

    size_t getObjectCount() const {
        return objects.size();
    }

    size_t getLightPointCount() const {
        return lightPoints.size();
    }

    int getFactionShipCount(int faction) const {
        if (faction >= 0 && faction < MAX_FACTIONS) {
            return factionShipCounts[faction];
        }
        return 0;
    }
};

// ====================== Spaceship成员函数实现 ======================
Spaceship::Spaceship(Vector2D pos, sf::Color col, int faction, IPhysicsSystem* physSys, float m, float h)
    : PhysicsObject(pos, m, h, faction), baseColor(col), size(5.0f),
    fireRate(2.0f), fireTimer(0), fireRange(120.0f), damage(10.0f), fireAngle(PI / 6),
    maneuverability(0.8f), maxSpeed(120.0f), accelerationRate(50.0f), brakeRate(20.0f),
    recoveryRate(1.0f), miningExpertise(0.1f),
    currentState(AIState::IDLE), previousState(AIState::IDLE), viewRange(180.0f),
    stateTimer(0), idleActionTimer(0),
    isBuilding(false), buildProgress(0), buildTimer(0), buildSpeed(0.1f),
    collectionDistance(10.0f),
    lastDecisionTime(0), decisionInterval(0.5f),
    isTurningIntentionally(false), turningTime(0), turningDuration(0),
    physicsSystem(physSys),
    level(1),
    upgradeCooldown(0), // 初始升级冷却为0
    randomEngine(std::random_device{}()),
    randomDist(0.0f, 1.0f) {


    // 创建等腰三角形（船头尖，指向x轴正方向）
    localVertices.resize(3);
    localVertices[0] = Vector2D(size, 0);                 // 船头（指向x轴正方向）
    localVertices[1] = Vector2D(-size * 0.8f, -size * 0.5f);  // 左后
    localVertices[2] = Vector2D(-size * 0.8f, size * 0.5f);   // 右后

    // 初始化飞船形状
    shape.setPointCount(3);
    shape.setOutlineThickness(0);

    // 初始化发光形状
    glowShape.setPointCount(3);
    glowShape.setOutlineThickness(0);

    // 初始化建造环
    buildRing.setRadius(size * 1.5f);
    buildRing.setFillColor(sf::Color::Transparent);
    buildRing.setOutlineColor(sf::Color(255, 255, 255, 200));
    buildRing.setOutlineThickness(2.0f);
    buildRing.setOrigin(size * 1.5f, size * 1.5f);
    buildRing.setPointCount(60);

    setRestitution(0.3f);

    // 初始速度 - 给予一个小的随机初始速度
    float initSpeed = 10.0f + randomDist(randomEngine) * 10.0f;
    float initAngle = randomDist(randomEngine) * 2.0f * PI;
    velocity = Vector2D(std::cos(initAngle), std::sin(initAngle)) * initSpeed;

    // 加载等级显示字体（仅第一次）
    if (!levelFontLoaded) {
        if (levelFont.loadFromFile("fonts/arial.ttf")) {
            levelFontLoaded = true;
        }
    }

    updateAppearance();
}

void Spaceship::update(float deltaTime) {
    PhysicsObject::update(deltaTime);

    // 每秒恢复生命值
    health = std::min(maxHealth, health + recoveryRate * deltaTime);

    // 确保速度不超过阈值
    if (velocity.length() > maxSpeed) {
        velocity = velocity.normalized() * maxSpeed;
    }

    // 更新主动转向计时器
    if (isTurningIntentionally) {
        turningTime += deltaTime;
        if (turningTime >= turningDuration) {
            isTurningIntentionally = false;
            turningTime = 0;
        }
    }

    updateAppearance();

    // AI决策
    updateAI(deltaTime);

    // 更新射击计时器
    fireTimer += deltaTime;

    // 更新激光
    for (auto it = lasers.begin(); it != lasers.end();) {
        it->update(deltaTime);
        if (!it->isAlive()) {
            it = lasers.erase(it);
        }
        else {
            ++it;
        }
    }

    // 状态计时器
    stateTimer += deltaTime;
    idleActionTimer += deltaTime;
    lastDecisionTime += deltaTime;

    // 更新建造进度
    if (isBuilding) {
        buildTimer += deltaTime;
        buildProgress = buildTimer * buildSpeed;

        // 建造过程中如果飞船被摧毁，停止建造
        if (!isActive) {
            isBuilding = false;
            buildProgress = 0;
            buildTimer = 0;
        }
        else if (buildProgress >= 1.0f) {
            // 建造完成
            buildProgress = 1.0f;
        }
    }

    // 关键修改：只有在空闲状态且速度达到阈值90%时才将船头转向移动方向
    if (!isTurningIntentionally && currentState == AIState::IDLE && !target.lock()) {
        float speedRatio = velocity.length() / maxSpeed;
        if (speedRatio >= 0.9f) {
            float targetAngle = velocity.angle();
            float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

            // 缓慢转向移动方向
            float maxRotation = maneuverability * deltaTime * 0.3f;
            angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
            rotation += angleDiff;
        }
    }
}

bool Spaceship::isInWindowView() const {
    return position.x >= 0 && position.x <= WINDOW_WIDTH &&
        position.y >= 0 && position.y <= WINDOW_HEIGHT;
}

bool Spaceship::checkFrontObstacle(float checkDistance) {
    // 检查前方是否有障碍物
    Vector2D shipDir = Vector2D(1, 0).rotated(rotation);
    Vector2D checkPoint = position + shipDir * checkDistance;

    // 检查窗口边界
    if (checkPoint.x < 0 || checkPoint.x > WINDOW_WIDTH ||
        checkPoint.y < 0 || checkPoint.y > WINDOW_HEIGHT) {
        return true;
    }

    // 检查其他物体
    if (!hasPhysicsSystem()) return false;

    auto allObjects = physicsSystem->getObjectsInWindow();
    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue;
        if (obj->getType() == "LightPoint") continue;

        Vector2D toObj = obj->getPosition() - position;
        float distance = toObj.length();
        float dotProduct = shipDir.dot(toObj.normalized());

        if (dotProduct > 0.866f && distance < checkDistance) {
            return true;
        }
    }

    return false;
}

void Spaceship::applyUpgrade(int upgradeType) {
    // 每次升级都增加等级（仅记录升级次数）
    level++;

    switch (upgradeType) {
    case 1: // 生存升级
        recoveryRate += 1.0f;        // 恢复力+1
        maxHealth += 20.0f;          // 生命值上限+20
        health = std::min(maxHealth, health + 20.0f); // 同时恢复生命值
        break;
    case 2: // 进攻升级
        damage += 5.0f;              // 伤害+5
        fireRate += 1.0f;            // 射速+1
        break;
    case 3: // 机动升级
        maxSpeed += 30.0f;           // 最大速度+30
        maneuverability += 0.2f;     // 机动性+0.2
        accelerationRate = 50.0f * (maneuverability / 0.8f); // 基于初始机动性0.8计算
        brakeRate = 20.0f * (maneuverability / 0.8f);
        break;
    case 4: // 生产升级
        buildSpeed += 0.05f;         // 建造速度+0.05
        miningExpertise += 0.05f;    // 采矿专精+0.05
        break;
    case 5: // 领域升级
        viewRange += 30.0f;          // 视野+30
        fireRange += 20.0f;          // 射程+20
        collectionDistance += 5.0f;  // 收集距离+5
        break;
    }
}

void Spaceship::collectNearbyLightPoints() {
    if (!hasPhysicsSystem()) return;

    auto allLightPoints = physicsSystem->getAllLightPoints();

    for (const auto& lightPoint : allLightPoints) {
        if (!lightPoint->getIsActive()) continue;

        float distance = (lightPoint->getPosition() - position).length();
        if (distance <= collectionDistance) {
            lightPoint->startCollecting(shared_from_this());

            if (physicsSystem) {
                float currentResource = physicsSystem->getFactionResource(factionId);
                physicsSystem->setFactionResource(factionId, currentResource + 1.0f);
            }
        }
    }
}

void Spaceship::updateAI(float deltaTime) {
    if (!isInWindowView()) {
        Vector2D windowCenter(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
        Vector2D toCenter = windowCenter - position;
        float targetAngle = toCenter.angle();
        float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

        float maxRotation = maneuverability * deltaTime;
        angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
        rotation += angleDiff;

        applyThrust(accelerationRate * 0.5f);
        return;
    }

    if (lastDecisionTime >= decisionInterval) {
        lastDecisionTime = 0;
        evaluateState();
    }

    switch (currentState) {
    case AIState::ATTACKING:
        attackingBehavior(deltaTime);
        break;
    case AIState::COLLECTING:
        collectingBehavior(deltaTime);
        break;
    case AIState::MINING:
        miningBehavior(deltaTime);
        break;
    case AIState::AVOIDING:
        avoidingBehavior(deltaTime);
        break;
    case AIState::IDLE:
        idleBehavior(deltaTime);
        break;
    case AIState::TURNING:
        turningBehavior(deltaTime);
        break;
    }
}

void Spaceship::evaluateState() {
    previousState = currentState;


    std::shared_ptr<PhysicsObject> enemy = findEnemyInView();
    if (enemy) {
        currentState = AIState::ATTACKING;
        target = enemy;
        return;
    }

    std::shared_ptr<LightPoint> lightPoint = findLightPointInView();
    if (lightPoint) {
        currentState = AIState::COLLECTING;
        target = lightPoint;
        return;
    }

    std::shared_ptr<PhysicsObject> asteroid = findAsteroidInView();
    if (asteroid) {
        currentState = AIState::MINING;
        target = asteroid;
        return;
    }

    if (checkFrontObstacle(30.0f)) {
        currentState = AIState::AVOIDING;
        target.reset();
        return;
    }

    currentState = AIState::IDLE;
    target.reset();
}

void Spaceship::avoidingBehavior(float deltaTime) {
    if (!checkFrontObstacle(30.0f)) {
        currentState = AIState::IDLE;
        return;
    }

    float targetAngle = rotation + PI;
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    float maxRotation = maneuverability * deltaTime * 2.0f;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    if (velocity.length() > maxSpeed * 0.3f) {
        applyBrake(0.8f);
    }

    if (std::abs(angleDiff) < 0.5f) {
        applyThrust(accelerationRate * 0.5f);
    }

    if (stateTimer > 0.5f) {
        currentState = AIState::IDLE;
        stateTimer = 0;
    }
}

void Spaceship::attackingBehavior(float deltaTime) {
    auto targetObj = target.lock();
    if (!targetObj || !targetObj->getIsActive() || targetObj->getType() != "Spaceship") {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    auto targetShip = std::dynamic_pointer_cast<Spaceship>(targetObj);
    if (targetShip && !targetShip->isInWindowView()) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    Vector2D toTarget = targetObj->getPosition() - position;
    float distance = toTarget.length();

    if (distance > viewRange) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    float targetAngle = toTarget.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    if (distance > fireRange * 0.7f) {
        applyThrust(accelerationRate);
    }
    else {
        if (velocity.length() < maxSpeed * 0.6f) {
            applyThrust(accelerationRate * 0.5f);
        }
    }

    if (std::abs(angleDiff) < fireAngle && distance <= fireRange) {
        tryAttack(targetObj);
    }
}

void Spaceship::collectingBehavior(float deltaTime) {
    auto targetObj = target.lock();
    if (!targetObj || !targetObj->getIsActive()) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    auto lightPoint = std::dynamic_pointer_cast<LightPoint>(targetObj);
    if (!lightPoint) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    if (!lightPoint->getIsActive()) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    Vector2D toTarget = lightPoint->getPosition() - position;
    float distance = toTarget.length();

    if (distance > collectionDistance * 3.0f) {
        applyBrake(0.5f);
    }

    float targetAngle = toTarget.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    if (distance > collectionDistance * 1.5f) {
        if (velocity.length() < maxSpeed * 0.3f) {
            applyThrust(accelerationRate * 0.3f);
        }
    }
    else if (distance > collectionDistance) {
        if (velocity.length() < maxSpeed * 0.2f) {
            applyThrust(accelerationRate * 0.1f);
        }
    }
    else {
        applyBrake(0.8f);
    }

    if (distance <= collectionDistance) {
        lightPoint->startCollecting(shared_from_this());

        if (hasPhysicsSystem()) {
            float currentResource = physicsSystem->getFactionResource(factionId);
            physicsSystem->setFactionResource(factionId, currentResource + 1.0f);
        }

        currentState = AIState::IDLE;
        target.reset();
    }
}

void Spaceship::miningBehavior(float deltaTime) {
    auto targetObj = target.lock();
    if (!targetObj || !targetObj->getIsActive() || targetObj->getType() != "Asteroid") {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    Vector2D toTarget = targetObj->getPosition() - position;
    float distance = toTarget.length();

    if (distance > viewRange) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    float targetAngle = toTarget.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    if (distance > fireRange * 0.8f) {
        applyThrust(accelerationRate * 0.5f);
    }
    else if (distance < fireRange * 0.4f) {
        applyBrake(0.5f);
    }

    if (std::abs(angleDiff) < fireAngle && distance <= fireRange) {
        tryAttack(targetObj);
    }
}

void Spaceship::idleBehavior(float deltaTime) {
    float speedRatio = velocity.length() / maxSpeed;
    if (speedRatio < 0.5f) {
        applyThrust(accelerationRate * 0.3f);
    }

    if (idleActionTimer > 0.3f + randomDist(randomEngine) * 1.2f) {
        float angleChange = (randomDist(randomEngine) - 0.5f) * PI * 0.1f * maneuverability * 0.8f;
        rotation += angleChange;
        isTurningIntentionally = true;
        turningTime = 0;
        turningDuration = 0.5f + randomDist(randomEngine) * 1.0f;

        if (std::abs(angleChange) > 0.05f && velocity.length() > maxSpeed * 0.3f) {
            applyBrake(0.3f + randomDist(randomEngine) * 0.3f);
        }

        if (randomDist(randomEngine) < 0.8f) {
            float thrust = accelerationRate * (0.3f + randomDist(randomEngine) * 0.7f);
            applyThrust(thrust);
        }
        else if (randomDist(randomEngine) < 0.1f) {
            applyBrake(0.3f + randomDist(randomEngine) * 0.5f);
        }

        idleActionTimer = 0;
    }

    if (velocity.length() > maxSpeed * 0.3f) {
        applyBrake(0.2f);
    }

    if (randomDist(randomEngine) < 0.2f * deltaTime * 60.0f) {
        applyThrust(accelerationRate * 0.1f);
    }
}

void Spaceship::turningBehavior(float deltaTime) {
    if (velocity.length() < maxSpeed * 0.5f) {
        applyThrust(accelerationRate * 0.3f);
    }
}

void Spaceship::applyThrust(float force) {
    Vector2D thrustDir = Vector2D(1, 0).rotated(rotation);
    Vector2D thrust = thrustDir * force;
    applyForce(thrust);
}

void Spaceship::applyBrake(float intensity) {
    if (velocity.length() < 0.1f) return;

    Vector2D brakeDir = -velocity.normalized();
    Vector2D brakeForce = brakeDir * brakeRate * intensity;

    applyForce(brakeForce);
}

std::vector<std::shared_ptr<PixelBlock>> Spaceship::getPixelBlocks() const {
    std::vector<std::shared_ptr<PixelBlock>> blocks;
    float pixelSize = 2.0f;

    auto worldVertices = getConvexHull();

    float minX = std::min({ worldVertices[0].x, worldVertices[1].x, worldVertices[2].x });
    float maxX = std::max({ worldVertices[0].x, worldVertices[1].x, worldVertices[2].x });
    float minY = std::min({ worldVertices[0].y, worldVertices[1].y, worldVertices[2].y });
    float maxY = std::max({ worldVertices[0].y, worldVertices[1].y, worldVertices[2].y });

    for (float x = minX; x <= maxX; x += pixelSize) {
        for (float y = minY; y <= maxY; y += pixelSize) {
            Vector2D point(x, y);

            if (pointInTriangle(point, worldVertices[0], worldVertices[1], worldVertices[2])) {
                Vector2D localPos = point - position;
                Vector2D unrotatedPos = localPos.rotated(-rotation);

                sf::Color pixelColor = baseColor;
                pixelColor.a = 200;
                blocks.push_back(std::make_shared<PixelBlock>(unrotatedPos, pixelSize, pixelColor));
            }
        }
    }

    return blocks;
}

std::vector<Vector2D> Spaceship::getConvexHull() const {
    std::vector<Vector2D> worldVertices;
    for (const auto& vertex : localVertices) {
        Vector2D rotated = vertex.rotated(rotation);
        worldVertices.push_back(position + rotated);
    }
    return worldVertices;
}

float Spaceship::getBoundingRadius() const {
    float maxDist = 0;
    for (const auto& vertex : localVertices) {
        float dist = vertex.length();
        if (dist > maxDist) maxDist = dist;
    }
    return maxDist;
}

bool Spaceship::tryAttack(std::shared_ptr<PhysicsObject> targetObj) {
    if (fireTimer < 1.0f / fireRate) return false;

    Vector2D toTarget = targetObj->getPosition() - position;
    float distance = toTarget.length();

    if (distance > fireRange) return false;

    Vector2D shipDir = Vector2D(1, 0).rotated(rotation);

    float angleDiff = std::acos(shipDir.dot(toTarget.normalized()));

    if (std::abs(angleDiff) > fireAngle) return false;

    Vector2D laserStart = position + shipDir * size;

    Vector2D laserEnd;

    if (targetObj->getType() == "Asteroid") {
        Vector2D toAsteroid = targetObj->getPosition() - laserStart;
        Vector2D surfacePoint = targetObj->getPosition() - toAsteroid.normalized() * targetObj->getBoundingRadius();
        laserEnd = surfacePoint;
    }
    else {
        laserEnd = targetObj->getPosition();
    }

    lasers.emplace_back(laserStart, laserEnd, baseColor, shared_from_this(), damage);

    if (hasPhysicsSystem()) {
        physicsSystem->addLaser(lasers.back());
    }

    fireTimer = 0;

    return true;
}

void Spaceship::draw(sf::RenderWindow& window) {
    if (isActive) {
        // 只在禁生成区内绘制
        if (isInNoGenZone()) {
            // 绘制激光
            for (const auto& laser : lasers) {
                laser.draw(window);
            }

            // 绘制发光效果
            window.draw(glowShape);

            // 绘制飞船主体
            window.draw(shape);

            // 绘制建造环（如果正在建造）
            if (isBuilding) {
                drawBuildRing(window);
            }

            // 绘制选中光圈
            if (getIsSelected()) {  // 修改这里：使用 getIsSelected() 而不是 isSelected
                sf::CircleShape selectionCircle(size * 2.0f);
                selectionCircle.setFillColor(sf::Color::Transparent);
                selectionCircle.setOutlineColor(sf::Color::White);
                selectionCircle.setOutlineThickness(2.0f);
                selectionCircle.setOrigin(size * 2.0f, size * 2.0f);
                selectionCircle.setPosition(position.toSFML());
                window.draw(selectionCircle);
            }

            // 绘制等级（如果开启） - 优化版：使用静态字体
            if (g_showLevel && levelFontLoaded) {
                sf::Text levelText;
                levelText.setFont(levelFont);
                levelText.setString(std::to_string(level));
                levelText.setCharacterSize(12);
                levelText.setFillColor(sf::Color::White);

                // 预计算文本边界以设置原点
                sf::FloatRect bounds = levelText.getLocalBounds();
                levelText.setOrigin(bounds.width / 2, bounds.height / 2);
                levelText.setPosition(position.x, position.y - size * 3.0f);
                window.draw(levelText);
            }
        }
    }
}

void Spaceship::drawBuildRing(sf::RenderWindow& window) {
    buildRing.setPosition(position.toSFML());

    int alpha = 50 + static_cast<int>(buildProgress * 150);
    sf::Color ringColor = sf::Color(255, 255, 255, static_cast<sf::Uint8>(alpha));
    buildRing.setOutlineColor(ringColor);

    window.draw(buildRing);

    if (buildProgress > 0) {
        float radius = size * 1.5f;
        float angle = buildProgress * 2.0f * PI;

        sf::VertexArray progressArray(sf::TriangleFan, 3);
        progressArray[0].position = position.toSFML();
        progressArray[0].color = sf::Color(255, 255, 255, 100);

        for (int i = 1; i <= 2; i++) {
            float segmentAngle = (i - 1) * angle;
            Vector2D point = position + Vector2D(std::cos(segmentAngle), std::sin(segmentAngle)) * radius;
            progressArray[i].position = point.toSFML();
            progressArray[i].color = sf::Color(255, 255, 255, 100);
        }

        window.draw(progressArray);
    }
}

void Spaceship::startBuilding() {
    if (!isBuilding) {
        isBuilding = true;
        buildTimer = 0;
        buildProgress = 0;
    }
}

std::vector<std::shared_ptr<PhysicsObject>> Spaceship::onDestroy() {
    std::vector<std::shared_ptr<PhysicsObject>> fragments;

    int fragmentCount = 1 + static_cast<int>(randomDist(randomEngine) * 2);

    for (int i = 0; i < fragmentCount; i++) {
        float angle = randomDist(randomEngine) * 2.0f * PI;
        float distance = size * (0.5f + randomDist(randomEngine) * 0.5f);

        Vector2D fragmentPos = position + Vector2D(std::cos(angle), std::sin(angle)) * distance;

        Vector2D fragmentVel = velocity + Vector2D(std::cos(angle), std::sin(angle)) *
            (3.0f + randomDist(randomEngine) * 5.0f);

        float fragmentSize = 6.0f;
        float fragmentMass = mass * 0.05f;
        float fragmentHealth = 10.0f;

        auto fragment = std::make_shared<Asteroid>(fragmentPos, fragmentVel, fragmentSize,
            fragmentMass, fragmentHealth, factionId, physicsSystem);
        fragment->setRotation(angle);
        fragment->setAngularVelocity(randomDist(randomEngine) * 2.0f - 1.0f);

        fragments.push_back(fragment);
    }

    return fragments;
}

std::vector<std::shared_ptr<LightPoint>> Spaceship::generateLightPoints() {
    std::vector<std::shared_ptr<LightPoint>> lightPoints;

    int lightPointCount = 2 + rand() % 6;

    if (isBuilding) {
        lightPointCount += 5 + rand() % 6;
    }

    for (int i = 0; i < lightPointCount; i++) {
        float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
        float distance = size * (1.0f + static_cast<float>(rand() % 100) / 100.0f);
        Vector2D offset = Vector2D(std::cos(angle), std::sin(angle)) * distance;

        Vector2D vel = velocity + offset.normalized() * (2.0f + static_cast<float>(rand() % 100) / 100.0f * 3.0f);

        auto lightPoint = std::make_shared<LightPoint>(position + offset, vel, baseColor);
        lightPoints.push_back(lightPoint);
    }

    return lightPoints;
}

bool Spaceship::isOutOfBounds() {
    float margin = getBoundingRadius();
    bool outOfWindow = false;

    if (position.x < margin) {
        position.x = margin;
        velocity.x = std::abs(velocity.x) * 0.5f;
        outOfWindow = true;
    }
    if (position.x > WINDOW_WIDTH - margin) {
        position.x = WINDOW_WIDTH - margin;
        velocity.x = -std::abs(velocity.x) * 0.5f;
        outOfWindow = true;
    }
    if (position.y < margin) {
        position.y = margin;
        velocity.y = std::abs(velocity.y) * 0.5f;
        outOfWindow = true;
    }
    if (position.y > WINDOW_HEIGHT - margin) {
        position.y = WINDOW_HEIGHT - margin;
        velocity.y = -std::abs(velocity.y) * 0.5f;
        outOfWindow = true;
    }

    return false;
}

bool Spaceship::pointInTriangle(Vector2D p, Vector2D a, Vector2D b, Vector2D c) const {
    Vector2D v0 = c - a;
    Vector2D v1 = b - a;
    Vector2D v2 = p - a;

    float dot00 = v0.dot(v0);
    float dot01 = v0.dot(v1);
    float dot02 = v0.dot(v2);
    float dot11 = v1.dot(v1);
    float dot12 = v1.dot(v2);

    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0) && (v >= 0) && (u + v <= 1);
}

std::shared_ptr<LightPoint> Spaceship::findLightPointInView() {
    if (!hasPhysicsSystem()) return nullptr;

    auto allLightPoints = physicsSystem->getAllLightPoints();
    std::shared_ptr<LightPoint> closestLightPoint = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& lightPoint : allLightPoints) {
        if (!lightPoint->getIsActive()) continue;

        float distance = (lightPoint->getPosition() - position).length();
        if (distance <= viewRange && distance < minDistance) {
            minDistance = distance;
            closestLightPoint = lightPoint;
        }
    }

    return closestLightPoint;
}

void Spaceship::updateAppearance() {
    float healthRatio = health / maxHealth;

    sf::Color shipColor = baseColor;
    sf::Color glow = baseColor;

    float brightness = 0.3f + 0.7f * healthRatio;
    shipColor.r = static_cast<sf::Uint8>(shipColor.r * brightness);
    shipColor.g = static_cast<sf::Uint8>(shipColor.g * brightness);
    shipColor.b = static_cast<sf::Uint8>(shipColor.b * brightness);

    glow.a = static_cast<sf::Uint8>(150 * healthRatio);
    glow.r = std::min(255, static_cast<int>(glow.r * (0.8f + 0.5f * healthRatio)));
    glow.g = std::min(255, static_cast<int>(glow.g * (0.8f + 0.5f * healthRatio)));
    glow.b = std::min(255, static_cast<int>(glow.b * (0.8f + 0.5f * healthRatio)));

    shape.setFillColor(shipColor);
    for (int i = 0; i < 3; i++) {
        Vector2D rotated = localVertices[i].rotated(rotation);
        shape.setPoint(i, sf::Vector2f(position.x + rotated.x, position.y + rotated.y));
    }

    glowShape.setFillColor(glow);
    float glowScale = 1.3f;
    for (int i = 0; i < 3; i++) {
        Vector2D rotated = localVertices[i].rotated(rotation) * glowScale;
        glowShape.setPoint(i, sf::Vector2f(position.x + rotated.x, position.y + rotated.y));
    }
}

std::shared_ptr<PhysicsObject> Spaceship::findObstacleInPath() {
    if (!hasPhysicsSystem()) return nullptr;

    Vector2D shipDir = Vector2D(1, 0).rotated(rotation);
    float checkDistance = 100.0f;
    Vector2D checkPoint = position + shipDir * checkDistance;

    auto allObjects = physicsSystem->getObjectsInWindow();
    std::shared_ptr<PhysicsObject> closestObstacle = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue;
        if (obj->getType() == "Asteroid") {
            Vector2D toObj = obj->getPosition() - position;
            float distance = toObj.length();
            float dotProduct = shipDir.dot(toObj.normalized());

            if (dotProduct > 0.8f && distance < checkDistance && distance < minDistance) {
                minDistance = distance;
                closestObstacle = obj;
            }
        }
    }

    return closestObstacle;
}

std::shared_ptr<PhysicsObject> Spaceship::findEnemyInView() {
    if (!hasPhysicsSystem()) return nullptr;

    auto allObjects = physicsSystem->getObjectsInWindow();
    std::shared_ptr<PhysicsObject> closestEnemy = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue;
        if (obj->getType() == "Spaceship" && obj->getFactionId() != factionId) {
            float distance = (obj->getPosition() - position).length();
            if (distance <= viewRange && distance < minDistance) {
                minDistance = distance;
                closestEnemy = obj;
            }
        }
    }

    return closestEnemy;
}

std::shared_ptr<PhysicsObject> Spaceship::findAsteroidInView() {
    if (!hasPhysicsSystem()) return nullptr;

    auto allObjects = physicsSystem->getObjectsInWindow();
    std::shared_ptr<PhysicsObject> closestAsteroid = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue;
        if (obj->getType() == "Asteroid") {
            float distance = (obj->getPosition() - position).length();
            if (distance <= viewRange && distance < minDistance) {
                minDistance = distance;
                closestAsteroid = obj;
            }
        }
    }

    return closestAsteroid;
}

// ====================== 视野渲染系统 ======================
class VisionRenderer {
private:
    PhysicsSystem* physicsSystem;
    sf::RenderTexture renderTexture;
    bool renderTextureInitialized;

public:
    VisionRenderer(PhysicsSystem* physSys) : physicsSystem(physSys), renderTextureInitialized(false) {
        if (renderTexture.create(WINDOW_WIDTH, WINDOW_HEIGHT)) {
            renderTextureInitialized = true;
        }
    }

    void drawVision(sf::RenderWindow& window) {
        if (g_visionDisplayMode == 4) return;  // 模式4不显示

        // 获取所有飞船
        auto allShips = physicsSystem->getAllSpaceships();

        // 按阵营分组
        std::map<int, std::vector<std::shared_ptr<Spaceship>>> factionShips;
        for (const auto& ship : allShips) {
            if (!ship->getIsActive()) continue;
            factionShips[ship->getFactionId()].push_back(ship);
        }

        // 根据显示模式渲染
        switch (g_visionDisplayMode) {
        case 0: // 半透明圆圈
            drawVisionMode0(window, factionShips);
            break;
        case 1: // 半透明实心圆
            drawVisionMode1(window, factionShips);
            break;
        case 2: // 合并圆圈
            drawVisionMode2(window, factionShips);
            break;
        case 3: // 合并实心圆
            drawVisionMode3(window, factionShips);
            break;
        case 5: // 完全不透明
            drawVisionMode5(window, factionShips);
            break;
        }
    }

private:
    // 模式0：半透明圆圈
    void drawVisionMode0(sf::RenderWindow& window, const std::map<int, std::vector<std::shared_ptr<Spaceship>>>& factionShips) {
        for (const auto& factionPair : factionShips) {
            int faction = factionPair.first;
            sf::Color color = getFactionColor(faction);

            for (const auto& ship : factionPair.second) {
                if (!ship->isInWindowView()) continue;

                sf::CircleShape circle = ship->getVisionCircle();
                circle.setFillColor(sf::Color::Transparent);
                circle.setOutlineColor(sf::Color(color.r, color.g, color.b, 100));
                circle.setOutlineThickness(1.0f);

                window.draw(circle);
            }
        }
    }

    // 模式1：半透明实心圆
    void drawVisionMode1(sf::RenderWindow& window, const std::map<int, std::vector<std::shared_ptr<Spaceship>>>& factionShips) {
        for (const auto& factionPair : factionShips) {
            int faction = factionPair.first;
            sf::Color color = getFactionColor(faction);

            for (const auto& ship : factionPair.second) {
                if (!ship->isInWindowView()) continue;

                sf::CircleShape circle = ship->getVisionCircle();
                circle.setFillColor(sf::Color(color.r, color.g, color.b, 50));
                circle.setOutlineColor(sf::Color(color.r, color.g, color.b, 100));
                circle.setOutlineThickness(1.0f);

                window.draw(circle);
            }
        }
    }

    // 模式2：合并圆圈
    void drawVisionMode2(sf::RenderWindow& window, const std::map<int, std::vector<std::shared_ptr<Spaceship>>>& factionShips) {
        if (!renderTextureInitialized) return;

        renderTexture.clear(sf::Color::Transparent);

        // 绘制所有飞船的视野圆圈到纹理
        for (const auto& factionPair : factionShips) {
            int faction = factionPair.first;
            sf::Color color = getFactionColor(faction);

            for (const auto& ship : factionPair.second) {
                if (!ship->isInWindowView()) continue;

                sf::CircleShape circle = ship->getVisionCircle();
                circle.setFillColor(sf::Color::Transparent);
                circle.setOutlineColor(sf::Color(color.r, color.g, color.b, 100));
                circle.setOutlineThickness(1.0f);

                renderTexture.draw(circle);
            }
        }

        renderTexture.display();

        // 绘制合并后的纹理
        sf::Sprite sprite(renderTexture.getTexture());
        window.draw(sprite);
    }

    // 模式3：合并实心圆
    void drawVisionMode3(sf::RenderWindow& window, const std::map<int, std::vector<std::shared_ptr<Spaceship>>>& factionShips) {
        if (!renderTextureInitialized) return;

        renderTexture.clear(sf::Color::Transparent);

        // 使用加法混合模式
        sf::RenderStates states;
        states.blendMode = sf::BlendAdd;

        // 绘制所有飞船的视野圆圈到纹理
        for (const auto& factionPair : factionShips) {
            int faction = factionPair.first;
            sf::Color color = getFactionColor(faction);

            for (const auto& ship : factionPair.second) {
                if (!ship->isInWindowView()) continue;

                sf::CircleShape circle = ship->getVisionCircle();
                circle.setFillColor(sf::Color(color.r, color.g, color.b, 30)); // 降低透明度
                circle.setOutlineColor(sf::Color::Transparent);

                renderTexture.draw(circle, states);
            }
        }

        renderTexture.display();

        // 绘制合并后的纹理
        sf::Sprite sprite(renderTexture.getTexture());
        window.draw(sprite);
    }

    // 模式5：完全不透明
    void drawVisionMode5(sf::RenderWindow& window, const std::map<int, std::vector<std::shared_ptr<Spaceship>>>& factionShips) {
        for (const auto& factionPair : factionShips) {
            int faction = factionPair.first;
            sf::Color color = getFactionColor(faction);

            for (const auto& ship : factionPair.second) {
                if (!ship->isInWindowView()) continue;

                sf::CircleShape circle = ship->getVisionCircle();
                circle.setFillColor(color);
                circle.setOutlineColor(color);
                circle.setOutlineThickness(1.0f);

                window.draw(circle);
            }
        }
    }

    sf::Color getFactionColor(int faction) {
        static const std::array<sf::Color, 4> factionColors = {
            sf::Color(255, 50, 50),      // 红色
            sf::Color(50, 150, 255),     // 蓝色
            sf::Color(50, 255, 100),     // 绿色
            sf::Color(255, 255, 50)      // 黄色
        };

        if (faction >= 0 && faction < 4) {
            return factionColors[faction];
        }
        return sf::Color::White;
    }
};

// ====================== UI系统 ======================
class UISystem {
private:
    GameFont font;
    bool showUI;
    std::vector<sf::Color> factionColors;
    std::vector<sf::String> factionNames;
    PhysicsSystem* physicsSystem;
    std::shared_ptr<Spaceship> selectedShip; // 选中的飞船
    sf::RectangleShape selectedInfoBackground; // 选中飞船信息背景
    sf::Text selectedInfoText; // 选中飞船信息文本

public:
    UISystem(PhysicsSystem* physSys) : showUI(false), physicsSystem(physSys), selectedShip(nullptr) {
        factionColors = {
            sf::Color(255, 50, 50),
            sf::Color(50, 150, 255),
            sf::Color(50, 255, 100),
            sf::Color(255, 255, 50)
        };

        factionNames = {
            L"红色阵营",
            L"蓝色阵营",
            L"绿色阵营",
            L"黄色阵营"
        };

        // 初始化选中飞船信息显示
        selectedInfoBackground.setFillColor(sf::Color(0, 0, 0, 180));
        selectedInfoBackground.setOutlineColor(sf::Color::White);
        selectedInfoBackground.setOutlineThickness(1);

        selectedInfoText.setFont(font.getFont());
        selectedInfoText.setCharacterSize(16);
        selectedInfoText.setFillColor(sf::Color::White);
    }

    void toggleUI() {
        showUI = !showUI;
    }

    void setUIVisible(bool visible) {
        showUI = visible;
    }

    // 处理鼠标点击
    void handleMouseClick(int mouseX, int mouseY) {
        Vector2D clickPos(static_cast<float>(mouseX), static_cast<float>(mouseY));

        // 取消之前的选中
        if (selectedShip) {
            selectedShip->setSelected(false);
            selectedShip = nullptr;
        }

        // 查找点击位置附近的飞船
        auto allShips = physicsSystem->getAllSpaceships();
        std::vector<std::shared_ptr<Spaceship>> nearbyShips;

        for (const auto& ship : allShips) {
            if (!ship->getIsActive()) continue;

            float distance = (ship->getPosition() - clickPos).length();
            if (distance <= 8.0f) {
                nearbyShips.push_back(ship);
            }
        }

        // 如果有附近飞船，随机选择一个
        if (!nearbyShips.empty()) {
            int randomIndex = rand() % nearbyShips.size();
            selectedShip = nearbyShips[randomIndex];
            selectedShip->setSelected(true);
        }
    }

    void update(float deltaTime) {
        // 处理叛乱系统
        if (g_rebellionEnabled) {
            processRebellion(deltaTime);
        }
    }

    void draw(sf::RenderWindow& window) {
        if (!font.isLoaded()) return;

        // 绘制选中飞船信息
        drawSelectedShipInfo(window);

        if (!showUI) return;

        const sf::Font& uiFont = font.getFont();

        std::vector<sf::Vector2f> corners = {
            sf::Vector2f(20, 20),
            sf::Vector2f(WINDOW_WIDTH - 200, 20),
            sf::Vector2f(20, WINDOW_HEIGHT - 100),
            sf::Vector2f(WINDOW_WIDTH - 200, WINDOW_HEIGHT - 100)
        };

        for (int faction = 0; faction < 4; faction++) {
            drawFactionInfo(window, uiFont, faction, corners[faction]);
        }

        // 显示视野模式
        drawVisionModeInfo(window, uiFont);

        // 显示等级显示状态
        drawLevelDisplayInfo(window, uiFont);

        // 显示叛乱系统状态
        drawRebellionSystemInfo(window, uiFont);
    }

private:
    void drawSelectedShipInfo(sf::RenderWindow& window) {
        if (!selectedShip || !selectedShip->getIsActive()) {
            // 如果没有选中飞船或飞船已被摧毁，清除选中状态
            if (selectedShip) {
                selectedShip->setSelected(false);
                selectedShip = nullptr;
            }
            return;
        }

        // 更新选中飞船信息
        std::string info = selectedShip->getAttributesString();
        selectedInfoText.setString(info);

        // 计算背景大小
        sf::FloatRect textBounds = selectedInfoText.getLocalBounds();
        selectedInfoBackground.setSize(sf::Vector2f(textBounds.width + 20, textBounds.height + 20));
        selectedInfoBackground.setPosition(10, 10);

        // 设置文本位置
        selectedInfoText.setPosition(20, 15);

        // 绘制
        window.draw(selectedInfoBackground);
        window.draw(selectedInfoText);
    }

    void drawFactionInfo(sf::RenderWindow& window, const sf::Font& font, int faction, sf::Vector2f position) {
        float resources = physicsSystem->getFactionResource(faction);
        int ships = physicsSystem->getFactionShipCount(faction);

        sf::RectangleShape background(sf::Vector2f(180, 80));
        background.setPosition(position);
        background.setFillColor(sf::Color(0, 0, 0, 150));
        background.setOutlineColor(factionColors[faction]);
        background.setOutlineThickness(2);
        window.draw(background);

        sf::CircleShape colorCircle(8);
        colorCircle.setPosition(position.x + 10, position.y + 15);
        colorCircle.setFillColor(factionColors[faction]);
        window.draw(colorCircle);

        sf::Text nameText;
        nameText.setFont(font);
        nameText.setString(factionNames[faction]);
        nameText.setCharacterSize(16);
        nameText.setFillColor(sf::Color::White);
        nameText.setPosition(position.x + 30, position.y + 10);
        window.draw(nameText);

        sf::Text shipText;
        shipText.setFont(font);
        shipText.setString(L"飞船: " + std::to_wstring(ships));
        shipText.setCharacterSize(14);
        shipText.setFillColor(sf::Color::White);
        shipText.setPosition(position.x + 30, position.y + 35);
        window.draw(shipText);

        sf::Text resourceText;
        resourceText.setFont(font);
        resourceText.setString(L"资源: " + std::to_wstring(static_cast<int>(resources)));
        resourceText.setCharacterSize(14);
        resourceText.setFillColor(sf::Color::White);
        resourceText.setPosition(position.x + 30, position.y + 55);
        window.draw(resourceText);
    }

    void drawVisionModeInfo(sf::RenderWindow& window, const sf::Font& font) {
        std::wstring modeText;
        switch (g_visionDisplayMode) {
        case 0: modeText = L"视野模式: 0"; break;
        case 1: modeText = L"视野模式: 1"; break;
        case 2: modeText = L"视野模式: 2 (合并圆圈)"; break;
        case 3: modeText = L"视野模式: 3 (合并实心圆)"; break;
        case 4: modeText = L"视野模式: 关"; break;
        case 5: modeText = L"视野模式: 4"; break;
        }

        sf::Text text;
        text.setFont(font);
        text.setString(modeText);
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);
        text.setPosition(WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT - 40);

        // 添加背景
        sf::RectangleShape background(sf::Vector2f(200, 30));
        background.setPosition(WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT - 40);
        background.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(background);

        window.draw(text);
    }

    void drawLevelDisplayInfo(sf::RenderWindow& window, const sf::Font& font) {
        std::wstring levelText = g_showLevel ? L"等级: 显示" : L"等级: 隐藏";

        sf::Text text;
        text.setFont(font);
        text.setString(levelText);
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);
        text.setPosition(420, WINDOW_HEIGHT - 40);

        // 添加背景
        sf::RectangleShape background(sf::Vector2f(150, 30));
        background.setPosition(420, WINDOW_HEIGHT - 40);
        background.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(background);

        window.draw(text);
    }

    void drawRebellionSystemInfo(sf::RenderWindow& window, const sf::Font& font) {
        std::wstring rebellionText = g_rebellionEnabled ? L"叛乱: 开启" : L"叛乱: 关闭";

        sf::Text text;
        text.setFont(font);
        text.setString(rebellionText);
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);
        text.setPosition(WINDOW_WIDTH - 570, WINDOW_HEIGHT - 40);

        // 添加背景
        sf::RectangleShape background(sf::Vector2f(150, 30));
        background.setPosition(WINDOW_WIDTH - 570, WINDOW_HEIGHT - 40);
        background.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(background);

        window.draw(text);
    }

    // 处理叛乱系统
    void processRebellion(float deltaTime) {
        static float rebellionTimer = 0.0f;
        rebellionTimer += deltaTime;

        // 每秒进行一次判定
        if (rebellionTimer >= 1.0f) {
            rebellionTimer = 0.0f;

            // 检查是否有空阵营
            std::vector<int> emptyFactions;
            for (int faction = 0; faction < 4; faction++) {
                if (physicsSystem->getFactionShipCount(faction) == 0) {
                    emptyFactions.push_back(faction);
                }
            }

            if (emptyFactions.empty()) {
                return; // 没有空阵营，不进行叛乱
            }

            // 每秒有0.01的概率开始叛乱判定
            if ((rand() % 10000) / 10000.0f < 0.01f) {
                // 随机选择一个空阵营作为叛乱目标
                int targetFaction = emptyFactions[rand() % emptyFactions.size()];

                // 随机选择一个有飞船的阵营作为源阵营
                std::vector<int> populatedFactions;
                for (int faction = 0; faction < 4; faction++) {
                    if (faction != targetFaction && physicsSystem->getFactionShipCount(faction) > 0) {
                        populatedFactions.push_back(faction);
                    }
                }

                if (populatedFactions.empty()) {
                    return; // 没有其他有飞船的阵营
                }

                int sourceFaction = populatedFactions[rand() % populatedFactions.size()];

                // 获取源阵营的所有飞船
                auto allShips = physicsSystem->getAllSpaceships();
                std::vector<std::shared_ptr<Spaceship>> sourceShips;

                for (const auto& ship : allShips) {
                    if (ship->getIsActive() && ship->getFactionId() == sourceFaction) {
                        sourceShips.push_back(ship);
                    }
                }

                if (sourceShips.empty()) {
                    return;
                }

                // 找到源阵营中等级最高的飞船
                int maxLevel = 0;
                for (const auto& ship : sourceShips) {
                    if (ship->getLevel() > maxLevel) {
                        maxLevel = ship->getLevel();
                    }
                }

                // 对每个飞船进行叛乱判定
                for (const auto& ship : sourceShips) {
                    // 每低一级，叛乱概率增加10%
                    int levelDiff = maxLevel - ship->getLevel();
                    float rebellionChance = 0.1f * levelDiff;

                    // 基础叛乱概率为5%
                    rebellionChance = std::max(0.05f, rebellionChance);

                    if ((rand() % 10000) / 10000.0f < rebellionChance) {
                        // 飞船叛乱，改变阵营
                        ship->setFactionId(targetFaction);

                        // 改变颜色
                        sf::Color newColor = factionColors[targetFaction];
                        ship->setBaseColor(newColor); // 使用新的setter方法
                    }
                }
            }
        }
    }
};

// ====================== 游戏主类 ======================
class Game {
private:
    sf::RenderWindow window;
    sf::Clock clock;
    sf::Clock gameClock;
    PhysicsSystem physicsSystem;
    UISystem uiSystem;
    VisionRenderer visionRenderer;

    // 随机数生成
    std::mt19937 rng;
    std::uniform_real_distribution<float> distPos;
    std::uniform_real_distribution<float> distVel;
    std::uniform_real_distribution<float> distSize;

    // 小行星生成
    float asteroidTimer;
    float timeSinceLastWave;
    int waveCount;

    // 新增：奇点相关
    Vector2D singularityPoint;      // 奇点坐标
    float singularityTimer;         // 奇点更新计时器
    float windowCenterX;           // 窗口中心X
    float windowCenterY;           // 窗口中心Y

public:
    Game(int width, int height)
        : window(sf::VideoMode(width, height), L"暗域光战", sf::Style::Close),
        physicsSystem(),
        uiSystem(&physicsSystem),
        visionRenderer(&physicsSystem),
        rng(std::random_device()()),
        distPos(0, 1),
        distVel(5, 15),
        distSize(15, 35),
        asteroidTimer(0),
        timeSinceLastWave(0),
        waveCount(0),
        singularityTimer(0) {

        sf::ContextSettings settings;
        settings.antialiasingLevel = 8;
        window.create(sf::VideoMode(width, height), L"暗域光战", sf::Style::Close, settings);
        window.setFramerateLimit(60);

        // 计算窗口中心
        windowCenterX = WINDOW_WIDTH / 2.0f;
        windowCenterY = WINDOW_HEIGHT / 2.0f;

        // 初始化奇点
        updateSingularity(true);

        initializeShips();
    }

    // 新增：检查点是否在禁生成区内
    bool isInNoGenZone(float x, float y) const {
        return x >= -NO_GEN_ZONE_MARGIN &&
            x <= WINDOW_WIDTH + NO_GEN_ZONE_MARGIN &&
            y >= -NO_GEN_ZONE_MARGIN &&
            y <= WINDOW_HEIGHT + NO_GEN_ZONE_MARGIN;
    }

    // 新增：检查点是否在生成区内
    bool isInGenZone(float x, float y) const {
        // 首先在事件边界内
        if (x < 0 || x > EVENT_BOUNDARY_WIDTH ||
            y < 0 || y > EVENT_BOUNDARY_HEIGHT) {
            return false;
        }

        // 然后不在禁生成区内
        return !isInNoGenZone(x, y);
    }

    // 新增：更新奇点
    void updateSingularity(bool initial = false) {
        Vector2D newSingularity;
        int attempts = 0;
        const int MAX_ATTEMPTS = 100;

        do {
            // 如果是初始生成，完全随机
            if (initial) {
                newSingularity.x = distPos(rng) * EVENT_BOUNDARY_WIDTH;
                newSingularity.y = distPos(rng) * EVENT_BOUNDARY_HEIGHT;
            }
            // 否则在旧奇点200范围内
            else {
                float angle = distPos(rng) * 2.0f * PI;
                float distance = distPos(rng) * 200.0f;
                newSingularity = singularityPoint +
                    Vector2D(std::cos(angle), std::sin(angle)) * distance;
            }

            attempts++;

            // 确保在生成区内
            if (isInGenZone(newSingularity.x, newSingularity.y)) {
                singularityPoint = newSingularity;
                break;
            }

        } while (attempts < MAX_ATTEMPTS);

        // 如果尝试失败，使用备选方案
        if (attempts >= MAX_ATTEMPTS) {
            // 在生成区内找一个相对安全的位置
            // 简单实现：事件边界减去禁生成区
            float minX = NO_GEN_ZONE_MARGIN;
            float maxX = EVENT_BOUNDARY_WIDTH - NO_GEN_ZONE_MARGIN;
            float minY = NO_GEN_ZONE_MARGIN;
            float maxY = EVENT_BOUNDARY_HEIGHT - NO_GEN_ZONE_MARGIN;

            if (maxX > minX && maxY > minY) {
                singularityPoint.x = minX + distPos(rng) * (maxX - minX);
                singularityPoint.y = minY + distPos(rng) * (maxY - minY);
            }
            else {
                // 如果还是失败，使用窗口外但事件边界内的位置
                singularityPoint.x = EVENT_BOUNDARY_WIDTH * 0.5f;
                singularityPoint.y = EVENT_BOUNDARY_HEIGHT * 0.5f;
            }
        }
    }

    void initializeShips() {
        float margin = 80.0f;

        std::vector<sf::Color> colors = {
            sf::Color(255, 50, 50),
            sf::Color(50, 150, 255),
            sf::Color(50, 255, 100),
            sf::Color(255, 255, 50)
        };

        for (int faction = 0; faction < 4; faction++) {
            Vector2D startPos;
            Vector2D direction;

            switch (faction) {
            case 0:
                startPos = Vector2D(margin, margin);
                direction = Vector2D(1, 1).normalized();
                break;
            case 1:
                startPos = Vector2D(WINDOW_WIDTH - margin, margin);
                direction = Vector2D(-1, 1).normalized();
                break;
            case 2:
                startPos = Vector2D(margin, WINDOW_HEIGHT - margin);
                direction = Vector2D(1, -1).normalized();
                break;
            case 3:
                startPos = Vector2D(WINDOW_WIDTH - margin, WINDOW_HEIGHT - margin);
                direction = Vector2D(-1, -1).normalized();
                break;
            }

            for (int i = 0; i < 5; i++) {
                Vector2D offset = direction * (i * 16.0f);
                Vector2D shipPos = startPos + offset;

                auto ship = std::make_shared<Spaceship>(shipPos, colors[faction], faction, &physicsSystem);

                Vector2D center(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
                Vector2D toCenter = center - ship->getPosition();
                ship->setRotation(toCenter.angle());

                ship->setAngularVelocity(0);

                Vector2D initialVel = toCenter.normalized() * (15.0f + (i * 2.0f));
                ship->setVelocity(initialVel);

                physicsSystem.addObject(ship);
            }
        }
    }

    void generateAsteroidWave() {
        float baseCount = 2.0f;
        float timeFactor = std::min(5.0f, timeSinceLastWave / 60.0f);
        int asteroidCount = static_cast<int>(baseCount + timeFactor);

        if (rand() % 100 < 2) {
            asteroidCount += 2 + rand() % 3;
        }

        asteroidCount = std::min(asteroidCount, 6);

        for (int i = 0; i < asteroidCount; i++) {
            addRandomAsteroid();
        }

        timeSinceLastWave = 0;
        waveCount++;
    }

    // 修改：重写小行星生成函数
    void addRandomAsteroid() {
        Vector2D position;
        Vector2D velocity;
        int attempts = 0;
        const int MAX_ATTEMPTS = 50;

        // 生成位置：距离奇点1000以内且位于生成区
        do {
            // 70%在300以内，30%在300-1000
            float maxDistance = ASTEROID_SPAWN_DISTANCE;
            if (rand() % 100 < 70) {
                maxDistance = ASTEROID_CLOSE_SPAWN_RADIUS;
            }

            float angle = distPos(rng) * 2.0f * PI;
            float distance = distPos(rng) * maxDistance;

            position = singularityPoint +
                Vector2D(std::cos(angle), std::sin(angle)) * distance;

            attempts++;

        } while ((!isInGenZone(position.x, position.y) ||
            position.x < 0 || position.x > EVENT_BOUNDARY_WIDTH ||
            position.y < 0 || position.y > EVENT_BOUNDARY_HEIGHT) &&
            attempts < MAX_ATTEMPTS);

        if (attempts >= MAX_ATTEMPTS) {
            // 生成失败，使用备用方案
            float minX = NO_GEN_ZONE_MARGIN;
            float maxX = EVENT_BOUNDARY_WIDTH - NO_GEN_ZONE_MARGIN;
            float minY = NO_GEN_ZONE_MARGIN;
            float maxY = EVENT_BOUNDARY_HEIGHT - NO_GEN_ZONE_MARGIN;

            if (maxX > minX && maxY > minY) {
                position.x = minX + distPos(rng) * (maxX - minX);
                position.y = minY + distPos(rng) * (maxY - minY);
            }
            else {
                return; // 生成失败，跳过
            }
        }

        // 速度方向：奇点指向窗口中心，偏移-15~15度
        Vector2D toCenter(windowCenterX - singularityPoint.x,
            windowCenterY - singularityPoint.y);
        float baseAngle = toCenter.angle();

        // -15~15度偏移
        float angleOffset = (distPos(rng) - 0.5f) * 30.0f * (PI / 180.0f);
        float finalAngle = baseAngle + angleOffset;

        // 速度大小
        float speed;

        // 随机决定小行星类型
        int asteroidTypeRoll = rand() % 1000;
        Asteroid::AsteroidType asteroidType = Asteroid::AsteroidType::NORMAL;
        float size;

        if (asteroidTypeRoll < 895) { // 89.5% 普通小行星
            asteroidType = Asteroid::AsteroidType::NORMAL;
            if (rand() % 100 < 70) {
                size = 25.0f + distSize(rng) * 0.5f;
                speed = distVel(rng) * 0.7f;
            }
            else if (rand() % 100 < 85) {
                size = 45.0f + distSize(rng);
                speed = distVel(rng) * 0.4f;
            }
            else {
                size = 10.0f + distSize(rng) * 0.3f;
                speed = distVel(rng) * 1.5f;
            }
        }
        else if (asteroidTypeRoll < 1000) { // 10.5% 极速小行星
            asteroidType = Asteroid::AsteroidType::FAST_TRAIL;
            size = 80.0f + static_cast<float>(rand() % 41);
            speed = 300.0f + distVel(rng) * 10.0f;
        }
        else { // 0.0% 引力小行星（暂时关闭此功能
            asteroidType = Asteroid::AsteroidType::GRAVITY;
            size = 200.0f + static_cast<float>(rand() % 101);
            speed = distVel(rng) * 0.2f;
        }

        velocity = Vector2D(std::cos(finalAngle), std::sin(finalAngle)) * speed;

        float mass = size * 20.0f;

        auto asteroid = std::make_shared<Asteroid>(position, velocity, size,
            mass, 200.0f, -1,
            &physicsSystem, asteroidType);

        asteroid->setAngularVelocity(static_cast<float>(rand() % 100) / 100.0f * 1.0f - 0.5f);

        physicsSystem.addObject(asteroid);
    }

    void run() {
        while (window.isOpen()) {
            handleEvents();
            update();
            render();
        }
    }

private:
    void handleEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    uiSystem.toggleUI();
                }

                if (event.key.code == sf::Keyboard::R) {
                    physicsSystem.clear();
                    initializeShips();
                    asteroidTimer = 0;
                    timeSinceLastWave = 0;
                    waveCount = 0;
                }

                if (event.key.code == sf::Keyboard::Tab) {
                    g_visionDisplayMode = (g_visionDisplayMode + 1) % 6;
                    std::cout << "视野显示模式: " << g_visionDisplayMode << std::endl;
                }

                if (event.key.code == sf::Keyboard::L) {
                    g_showLevel = !g_showLevel;
                    std::cout << "等级显示: " << (g_showLevel ? "开启" : "关闭") << std::endl;
                }

                if (event.key.code == sf::Keyboard::P) {
                    g_rebellionEnabled = !g_rebellionEnabled;
                    std::cout << "叛乱系统: " << (g_rebellionEnabled ? "开启" : "关闭") << std::endl;
                }
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    uiSystem.handleMouseClick(event.mouseButton.x, event.mouseButton.y);
                }
            }
        }
    }

    void update() {
        float deltaTime = clock.restart().asSeconds();
        deltaTime = std::min(deltaTime, 0.1f);

        timeSinceLastWave += deltaTime;

        // 更新奇点计时器
        singularityTimer += deltaTime;
        if (singularityTimer >= SINGULARITY_UPDATE_TIME) {
            updateSingularity();
            singularityTimer = 0;
        }

        physicsSystem.update(deltaTime);
        uiSystem.update(deltaTime);

        asteroidTimer += deltaTime;
        float baseInterval = 16.0f;
        float minInterval = 6.0f;
        float interval = std::max(minInterval, baseInterval - static_cast<float>(waveCount) * 0.1f);

        if (asteroidTimer >= interval) {
            generateAsteroidWave();
            asteroidTimer = 0;
        }
    }

    void render() {
        window.clear(sf::Color::Black);

        // 绘制视野
        visionRenderer.drawVision(window);

        // 绘制所有物理对象和光点
        physicsSystem.draw(window);

        // 绘制UI
        uiSystem.draw(window);

        window.display();
    }
};

// ====================== 主函数 ======================
int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

#ifdef _WIN32
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        ShowWindow(consoleWnd, SW_HIDE);
        FreeConsole();
    }
#endif

    Game game(WINDOW_WIDTH, WINDOW_HEIGHT);
    game.run();

    return 0;
}