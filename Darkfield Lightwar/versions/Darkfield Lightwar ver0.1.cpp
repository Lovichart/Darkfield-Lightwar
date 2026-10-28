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
// 重要修改记录：
// 1. 修复光点收集资源增长问题 - 确保资源在收集完成后立即增长
// 2. 修复CollectionBeam构造函数中的编译错误
// 3. 修复激光攻击小行星的问题
// 4. 修复收集光束无限拉长问题 - 光束长度固定，光点沿光束移动
// 5. 修复光点收集过程中碰撞问题 - 收集时无物理碰撞
// 6. 修复飞船制动功能 - 改为移动方向的反方向阻力
// 7. 修复飞船建造完成后不生成新飞船的问题
// 8. 改进建造环显示为环形进度条
// 9. 防止多船同时采集同一光点
//
// 2024-01-20 重大更新：
// 1. 飞船增加"收集距离"属性，初始10.0f
// 2. 删除所有收集光束相关代码，简化收集逻辑
// 3. 更新收集模式AI行为：制动减速、转向对准、低速靠近
// 4. 重写建造系统，增加"建造速度"属性
// 5. 删除不再需要的元素：采集能力、收集光束等
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

// ====================== 游戏常量 ======================
constexpr float PI = 3.14159265358979323846f;
constexpr float TIME_STEP = 1.0f / 60.0f;    // 固定时间步长，确保物理稳定
constexpr int WINDOW_WIDTH = 1600;           // 窗口宽度
constexpr int WINDOW_HEIGHT = 900;           // 窗口高度
constexpr int SPAWN_AREA_WIDTH = 2400;       // 生成区宽度
constexpr int SPAWN_AREA_HEIGHT = 1350;      // 生成区高度
constexpr float RESOURCE_TO_BUILD = 10.0f;   // 建造新飞船所需资源（已修改为10）
constexpr float BUILD_TIME = 10.0f;          // 建造时间（秒）
constexpr float REINFORCEMENT_ZONE_INTERVAL = 30.0f; // 增援区域生成间隔
constexpr float REINFORCEMENT_ZONE_DURATION = 12.0f; // 增援区域持续时间

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
    sf::RectangleShape shape;
    Vector2D localPosition;
    bool active;
    sf::Color color;
    float size;

public:
    PixelBlock(Vector2D localPos, float pixelSize, sf::Color col)
        : localPosition(localPos), active(true), color(col), size(pixelSize) {

        shape.setSize(sf::Vector2f(size, size));
        shape.setFillColor(color);
        shape.setOutlineThickness(0);
        shape.setOrigin(size / 2, size / 2);
    }

    void draw(sf::RenderWindow& window, Vector2D parentPos, float parentRotation = 0) {
        if (active) {
            Vector2D rotatedPos = localPosition.rotated(parentRotation);
            shape.setPosition(parentPos.x + rotatedPos.x,
                parentPos.y + rotatedPos.y);
            shape.setRotation(parentRotation * 180.0f / PI);
            window.draw(shape);
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

    // 检查是否飞出生成区
    virtual bool isOutOfBounds() {
        float margin = 100.0f;
        return (position.x < -margin || position.x > SPAWN_AREA_WIDTH + margin ||
            position.y < -margin || position.y > SPAWN_AREA_HEIGHT + margin);
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
};

// ====================== 光点类（原Resource类） ======================
class LightPoint : public PhysicsObject {
private:
    sf::Color color;
    float radius;
    sf::CircleShape shape;
    bool beingCollected; // 是否正在被收集
    std::weak_ptr<PhysicsObject> collector; // 收集者（如果有）
    bool collectionCompleted; // 收集是否完成
    bool collectionLocked; // 是否被锁定（防止多船同时采集）
    bool ignorePhysicsDuringCollection; // 收集时忽略物理

public:
    LightPoint(Vector2D pos, Vector2D vel, sf::Color col)
        : PhysicsObject(pos, 0.5f, 1.0f, -1), color(col), radius(2.0f),
        beingCollected(false), collectionCompleted(false),
        collectionLocked(false), ignorePhysicsDuringCollection(false) {

        velocity = vel;

        shape.setRadius(radius);
        shape.setFillColor(color);
        shape.setOutlineThickness(0);
        shape.setOrigin(radius, radius);
    }

    void update(float deltaTime) override {
        if (!isActive) return;

        // 如果收集完成，标记为不活跃
        if (collectionCompleted) {
            isActive = false;
            return;
        }

        if (beingCollected) {
            auto collectorPtr = collector.lock();
            if (collectorPtr) {
                // 被收集时，忽略物理更新
                ignorePhysicsDuringCollection = true;
                // 立即标记为收集完成
                collectionCompleted = true;
                isActive = false;
            }
            else {
                // 收集者失效，停止收集
                beingCollected = false;
                collectionCompleted = false;
                collectionLocked = false;
                ignorePhysicsDuringCollection = false;
            }
        }
        else {
            // 正常光点的物理更新
            ignorePhysicsDuringCollection = false;

            // 轻微阻力
            velocity = velocity * 0.98f;
            position = position + velocity * deltaTime;
            shape.setPosition(position.toSFML());
        }
    }

    // 获取像素块（光点用单个像素块近似）
    std::vector<std::shared_ptr<PixelBlock>> getPixelBlocks() const override {
        if (beingCollected || ignorePhysicsDuringCollection) {
            // 收集过程中不参与碰撞检测
            return {};
        }

        std::vector<std::shared_ptr<PixelBlock>> blocks;

        // 创建一个近似圆形的像素块（简单处理，用4个像素块）
        float blockSize = radius * 0.7f;
        std::vector<Vector2D> offsets = {
            Vector2D(0, 0),
            Vector2D(blockSize, 0),
            Vector2D(-blockSize, 0),
            Vector2D(0, blockSize),
            Vector2D(0, -blockSize)
        };

        for (const auto& offset : offsets) {
            auto block = std::make_shared<PixelBlock>(offset, blockSize, color);
            blocks.push_back(block);
        }

        return blocks;
    }

    // 获取凸包顶点（圆形近似为8边形）
    std::vector<Vector2D> getConvexHull() const override {
        if (beingCollected || ignorePhysicsDuringCollection) {
            // 收集过程中不参与碰撞检测
            return {};
        }

        std::vector<Vector2D> hull;
        int segments = 8;

        for (int i = 0; i < segments; i++) {
            float angle = 2.0f * PI * i / segments;
            Vector2D point = Vector2D(std::cos(angle), std::sin(angle)) * radius;
            hull.push_back(position + point);
        }

        return hull;
    }

    // 获取外接圆半径
    float getBoundingRadius() const override {
        if (beingCollected || ignorePhysicsDuringCollection) {
            // 收集过程中不参与碰撞检测
            return 0;
        }
        return radius;
    }

    void draw(sf::RenderWindow& window) override {
        if (isActive && !collectionCompleted) {
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

    // 检查是否可以被采集（光点总是可以被采集，但需要没有被锁定）
    bool isCollectible() const override { return !collectionLocked && !beingCollected; }

    // 开始收集
    void startCollecting(std::shared_ptr<PhysicsObject> collectorObj) override {
        // 确保光点没有被锁定
        if (collectionLocked) return;

        beingCollected = true;
        collector = collectorObj;
        collectionCompleted = false;
        collectionLocked = true; // 锁定光点，防止其他飞船采集
        ignorePhysicsDuringCollection = true; // 收集时忽略物理
        velocity = Vector2D(0, 0); // 停止移动
    }

    // 结束收集
    void stopCollecting() override {
        beingCollected = false;
        collector.reset();
        collectionCompleted = false;
        collectionLocked = false; // 解锁光点
        ignorePhysicsDuringCollection = false; // 恢复物理
    }

    // 获取收集进度
    float getCollectProgress() const override {
        return collectionCompleted ? 1.0f : 0.0f;
    }

    // 是否正在被收集
    bool isBeingCollected() const { return beingCollected; }

    // 收集是否完成
    bool isCollectionCompleted() const { return collectionCompleted; }

    // 是否被锁定
    bool isLocked() const { return collectionLocked; }

    // 获取像素数量（光点价值为1）
    int getPixelCount() const override { return 1; }

    // 获取颜色
    sf::Color getColor() const { return color; }

    // 获取收集者
    std::shared_ptr<PhysicsObject> getCollector() const { return collector.lock(); }

    // 强制解锁（用于异常情况）
    void forceUnlock() { collectionLocked = false; }

    // 检查是否在收集过程中忽略物理
    bool shouldIgnorePhysics() const { return ignorePhysicsDuringCollection; }
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

        sf::VertexArray glowQuad(sf::Quads, 4);
        glowQuad[0].position = (startPos - perp).toSFML();
        glowQuad[1].position = (startPos + perp).toSFML();
        glowQuad[2].position = (endPos + perp).toSFML();
        glowQuad[3].position = (endPos - perp).toSFML();

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

// ====================== 三角形飞船类 ======================
class Spaceship : public PhysicsObject {
private:
    sf::ConvexShape shape;
    sf::ConvexShape glowShape;
    sf::CircleShape viewCircle; // 视野圆圈
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

    // AI状态
    enum class AIState {
        AVOIDING,       // 避障状态
        ATTACKING,      // 攻击敌对飞船
        COLLECTING,     // 采集光点
        MINING,         // 攻击小行星
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
    std::vector<std::weak_ptr<LightPoint>> nearbyLightPoints; // 附近的光点

    // 最近一次决定的时间（用于随机决策）
    float lastDecisionTime;
    float decisionInterval;

    // 主动转向相关
    bool isTurningIntentionally;  // 是否正在主动转向
    float turningTime;           // 转向计时器
    float turningDuration;       // 转向持续时间

    // 物理系统引用（用于查询其他对象）
    class PhysicsSystem* physicsSystem;

    // 独立随机数生成器
    std::mt19937 randomEngine;
    std::uniform_real_distribution<float> randomDist;

public:
    Spaceship(Vector2D pos, sf::Color col, int faction, class PhysicsSystem* physSys, float m = 3.0f, float h = 100.0f);

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

private:
    // 检查点是否在三角形内（重心坐标法）
    bool pointInTriangle(Vector2D p, Vector2D a, Vector2D b, Vector2D c) const;

    void updateAppearance();

    // 清理无效的采集目标
    void cleanupNearbyLightPoints();
};

// ====================== 增援区域类 ======================
class ReinforcementZone {
private:
    Vector2D position;
    float lifetime; // 当前剩余时间
    float maxLifetime; // 最大生存时间
    float currentRadius; // 当前半径
    float maxRadius; // 最大半径
    bool active;

    // 装饰元素
    sf::CircleShape zoneCircle;
    sf::ConvexShape shipIndicator;
    float shipRotation; // 飞船标识旋转角度

    // 虚线效果相关
    std::vector<sf::VertexArray> dashedLines;
    float dashLength;
    float gapLength;

public:
    ReinforcementZone(Vector2D pos, float duration = REINFORCEMENT_ZONE_DURATION)
        : position(pos), maxLifetime(duration), lifetime(duration), active(true),
        maxRadius(100.0f), currentRadius(5.0f), shipRotation(0),
        dashLength(10.0f), gapLength(5.0f) {

        // 初始化虚线圆圈
        updateDashedCircle();

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

    void update(float deltaTime) {
        if (!active) return;

        lifetime -= deltaTime;

        if (lifetime <= 0) {
            active = false;
            return;
        }

        // 根据时间阶段调整半径
        // 阶段1: 12s-8s (0-4s) 变大
        // 阶段2: 8s-4s (4-8s) 保持不变
        // 阶段3: 4s-0s (8-12s) 缩小

        float phaseTime = maxLifetime - lifetime; // 已过时间

        if (phaseTime <= 4.0f) {
            // 变大阶段
            float t = phaseTime / 4.0f;
            currentRadius = 5.0f + (maxRadius - 5.0f) * t;
        }
        else if (phaseTime <= 8.0f) {
            // 保持阶段
            currentRadius = maxRadius;
        }
        else {
            // 缩小阶段
            float t = (phaseTime - 8.0f) / 4.0f;
            currentRadius = maxRadius * (1.0f - t);
        }

        // 更新虚线圆圈
        updateDashedCircle();

        // 更新飞船标识旋转
        shipRotation += deltaTime * 0.5f; // 缓慢旋转
        if (shipRotation > 2.0f * PI) shipRotation -= 2.0f * PI;

        // 更新飞船标识位置（在圆圈内随机位置，但每帧固定）
        float indicatorAngle = shipRotation * 2.0f; // 旋转速度不同
        float indicatorDistance = currentRadius * 0.7f;
        Vector2D indicatorOffset(std::cos(indicatorAngle), std::sin(indicatorAngle));
        indicatorOffset = indicatorOffset * indicatorDistance;

        shipIndicator.setPosition(position.x + indicatorOffset.x,
            position.y + indicatorOffset.y);
        shipIndicator.setRotation(shipRotation * 180.0f / PI);
    }

    void draw(sf::RenderWindow& window) const {
        if (!active) return;

        // 绘制虚线圆圈
        for (const auto& line : dashedLines) {
            window.draw(line);
        }

        // 绘制飞船标识
        window.draw(shipIndicator);

        // 绘制区域中心点（可选）
        sf::CircleShape centerDot(3.0f);
        centerDot.setFillColor(sf::Color::White);
        centerDot.setOrigin(3.0f, 3.0f);
        centerDot.setPosition(position.toSFML());
        window.draw(centerDot);
    }

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

private:
    // 更新虚线圆圈
    void updateDashedCircle() {
        dashedLines.clear();

        int segments = 60; // 圆圈分割段数
        float angleStep = 2.0f * PI / segments;

        for (int i = 0; i < segments; i++) {
            float startAngle = i * angleStep;
            float endAngle = (i + 1) * angleStep;

            // 创建虚线效果：每段间隔绘制
            if (i % 3 == 0) { // 每3段绘制1段，形成虚线效果
                Vector2D startPos = position + Vector2D(std::cos(startAngle), std::sin(startAngle)) * currentRadius;
                Vector2D endPos = position + Vector2D(std::cos(endAngle), std::sin(endAngle)) * currentRadius;

                sf::VertexArray line(sf::Lines, 2);
                line[0].position = startPos.toSFML();
                line[0].color = sf::Color::White;
                line[1].position = endPos.toSFML();
                line[1].color = sf::Color::White;

                dashedLines.push_back(line);
            }
        }
    }
};

// ====================== 小行星类（像素方块组成） ======================
class Asteroid : public PhysicsObject {
private:
    std::vector<std::shared_ptr<PixelBlock>> pixelBlocks;
    sf::Color baseColor;
    float pixelSize;
    bool canSplit;

    // 用于碰撞检测的凸包顶点
    mutable std::vector<Vector2D> cachedConvexHull;
    mutable bool hullDirty;

public:
    Asteroid(Vector2D pos, Vector2D vel, float radius, float m = 50.0f, float h = 200.0f, int faction = -1)
        : PhysicsObject(pos, m, h, faction), pixelSize(4.0f), canSplit(true), hullDirty(true) {

        velocity = vel;
        setRestitution(0.1f);

        // 小行星颜色（灰色系）
        int grayValue = 120 + rand() % 60;
        int colorVar = rand() % 40 - 20;
        baseColor = sf::Color(
            std::clamp(grayValue + colorVar, 80, 200),
            std::clamp(grayValue + colorVar, 80, 200),
            std::clamp(grayValue + colorVar, 80, 200)
        );

        // 生成像素方块组成的小行星
        generatePixelAsteroid(radius);

        // 更新质量和血量（基于像素数量）
        updateMassAndHealthFromPixels();
    }

    void update(float deltaTime) override {
        PhysicsObject::update(deltaTime);
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
        return maxDist + pixelSize;
    }

    void takeDamage(float damage, Vector2D damagePoint = Vector2D(0, 0), std::shared_ptr<PhysicsObject> attacker = nullptr) override {
        if (!isActive) return;

        // 应用伤害
        health -= damage;

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
        causeEdgeDamage(damagePoint, damage);

        // 更新质量和血量
        updateMassAndHealthFromPixels();

        // 标记凸包需要更新
        hullDirty = true;
    }

    void draw(sf::RenderWindow& window) override {
        if (isActive) {
            for (const auto& pixel : pixelBlocks) {
                if (pixel->isActive()) {
                    pixel->draw(window, position, rotation);
                }
            }
        }
    }

    // 获取对象类型标识
    std::string getType() const override { return "Asteroid"; }

    // 检查是否可以被采集（小行星不可被采集，只有光点可以被采集）
    bool isCollectible() const override { return false; }

    // 获取像素数量
    int getPixelCount() const override {
        return countActivePixels();
    }

    // 小行星破碎
    std::vector<std::shared_ptr<PhysicsObject>> onDestroy() override {
        std::vector<std::shared_ptr<PhysicsObject>> fragments;

        if (pixelBlocks.size() <= 1) {
            // 只有一个像素块，不分裂
            return fragments;
        }

        // 根据剩余像素块数量决定分裂成几块
        int activePixels = countActivePixels();
        if (activePixels == 0) return fragments;

        // 分裂成2-4块
        int fragmentCount = std::min(4, std::max(2, activePixels / 10));

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

        // 创建碎片
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

            // 计算碎片速度（继承原速度加上爆炸速度）
            Vector2D explosionDir = fragmentCenter.normalized();
            Vector2D fragmentVel = velocity + explosionDir * (2.0f + static_cast<float>(rand() % 100) / 50.0f);

            // 创建新小行星
            float fragmentRadius = calculateGroupRadius(pixelGroups[i], fragmentCenter);
            float fragmentMass = mass * (static_cast<float>(pixelGroups[i].size()) / static_cast<float>(activePixels));
            float fragmentHealth = maxHealth * (static_cast<float>(pixelGroups[i].size()) / static_cast<float>(activePixels));

            auto fragment = std::make_shared<Asteroid>(fragmentWorldPos, fragmentVel, fragmentRadius,
                fragmentMass, fragmentHealth);

            // 设置碎片像素块
            fragment->pixelBlocks.clear();
            for (const auto& pixel : pixelGroups[i]) {
                Vector2D localPos = pixel->getLocalPosition() - fragmentCenter;
                fragment->pixelBlocks.push_back(std::make_shared<PixelBlock>(
                    localPos, pixelSize, pixel->getColor()));
            }

            // 设置旋转
            fragment->setRotation(rotation + static_cast<float>(rand() % 100) / 100.0f * PI);
            fragment->setAngularVelocity(angularVelocity + static_cast<float>(rand() % 100) / 100.0f * 2.0f - 1.0f);

            fragments.push_back(fragment);
        }

        return fragments;
    }

    // 生成光点
    std::vector<std::shared_ptr<LightPoint>> generateLightPoints() override {
        std::vector<std::shared_ptr<LightPoint>> lightPoints;

        // 小行星被摧毁时生成0-5个光点
        int lightPointCount = rand() % 6; // 0到5个

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

private:
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

    void causeEdgeDamage(Vector2D damagePoint, float damage) {
        // 将伤害点转换到本地坐标系
        Vector2D localDamagePoint = (damagePoint - position).rotated(-rotation);

        // 计算要移除的像素块数量
        int pixelsToRemove = static_cast<int>(damage / 10.0f);
        pixelsToRemove = std::min(pixelsToRemove, countActivePixels() / 3);

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

        // 移除最近的像素块
        for (int i = 0; i < std::min(pixelsToRemove, static_cast<int>(sortedPixels.size())); i++) {
            sortedPixels[i]->setActive(false);
        }
    }

    void updateMassAndHealthFromPixels() {
        int activeCount = countActivePixels();
        if (activeCount == 0) {
            isActive = false;
            return;
        }

        // 更新血量（与活跃像素数成正比）
        health = maxHealth * (static_cast<float>(activeCount) / static_cast<float>(pixelBlocks.size()));

        // 更新质量（与像素数成正比）
        mass = 50.0f * (static_cast<float>(activeCount) / static_cast<float>(pixelBlocks.size()));
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
};

// ====================== 精确碰撞检测系统 ======================
class PhysicsSystem {
private:
    std::vector<std::shared_ptr<PhysicsObject>> objects;
    std::vector<std::shared_ptr<LightPoint>> lightPoints; // 光点列表
    std::vector<std::shared_ptr<PhysicsObject>> objectsToAdd;
    std::vector<std::shared_ptr<LightPoint>> lightPointsToAdd;

    // 增援区域列表
    std::vector<std::shared_ptr<ReinforcementZone>> reinforcementZones;
    std::vector<std::shared_ptr<ReinforcementZone>> zonesToAdd;

    // 激光列表
    std::vector<LaserBeam> lasers;

    // 空间分区网格
    static const int GRID_SIZE = 100;
    static const int GRID_WIDTH = 24;  // 2400/100 = 24
    static const int GRID_HEIGHT = 14; // 1350/100 ≈ 14
    std::vector<std::vector<std::vector<std::shared_ptr<PhysicsObject>>>> spatialGrid;

    // 阵营资源统计（按阵营ID索引）
    static const int MAX_FACTIONS = 4;
    std::array<float, MAX_FACTIONS> factionResources;
    std::array<int, MAX_FACTIONS> factionShipCounts;

    // 增援区域生成计时器
    float reinforcementZoneTimer;

    // 精确像素碰撞检测
    bool checkPixelCollision(std::shared_ptr<PhysicsObject> a, std::shared_ptr<PhysicsObject> b) {
        // 如果任意一个是被收集的光点，跳过碰撞检测
        if (a->getType() == "LightPoint") {
            auto lightPointA = std::dynamic_pointer_cast<LightPoint>(a);
            if (lightPointA && lightPointA->shouldIgnorePhysics()) return false;
        }
        if (b->getType() == "LightPoint") {
            auto lightPointB = std::dynamic_pointer_cast<LightPoint>(b);
            if (lightPointB && lightPointB->shouldIgnorePhysics()) return false;
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

    // 计算碰撞响应
    void resolveCollision(std::shared_ptr<PhysicsObject> a, std::shared_ptr<PhysicsObject> b,
        Vector2D collisionPoint, Vector2D normal) {
        // 如果任意一个是被收集的光点，跳过碰撞响应
        if (a->getType() == "LightPoint") {
            auto lightPointA = std::dynamic_pointer_cast<LightPoint>(a);
            if (lightPointA && lightPointA->shouldIgnorePhysics()) return;
        }
        if (b->getType() == "LightPoint") {
            auto lightPointB = std::dynamic_pointer_cast<LightPoint>(b);
            if (lightPointB && lightPointB->shouldIgnorePhysics()) return;
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

        // 计算伤害
        float kineticEnergy = 0.5f * a->getMass() * a->getVelocity().lengthSquared() +
            0.5f * b->getMass() * b->getVelocity().lengthSquared();

        float damage = kineticEnergy * 0.002f;

        // 应用伤害
        a->takeDamage(damage, collisionPoint);
        b->takeDamage(damage, collisionPoint);
    }

    // 更新空间分区网格
    void updateSpatialGrid() {
        // 清空网格
        for (auto& row : spatialGrid) {
            for (auto& cell : row) {
                cell.clear();
            }
        }

        // 将物体放入网格（包括光点）
        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;

            // 跳过被收集的光点（不参与碰撞检测）
            if (obj->getType() == "LightPoint") {
                auto lightPoint = std::dynamic_pointer_cast<LightPoint>(obj);
                if (lightPoint && lightPoint->shouldIgnorePhysics()) {
                    continue;
                }
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

    // 获取物体周围的候选碰撞对象
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
                            // 跳过被收集的光点
                            if (nearbyObj->getType() == "LightPoint") {
                                auto lightPoint = std::dynamic_pointer_cast<LightPoint>(nearbyObj);
                                if (lightPoint && lightPoint->shouldIgnorePhysics()) {
                                    continue;
                                }
                            }
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

    // 处理增援区域
    void processReinforcementZones(float deltaTime) {
        // 更新增援区域生成计时器
        reinforcementZoneTimer += deltaTime;

        // 每30秒生成一个增援区域
        if (reinforcementZoneTimer >= REINFORCEMENT_ZONE_INTERVAL) {
            // 在窗口区域内随机生成位置
            float x = 100.0f + static_cast<float>(rand() % (WINDOW_WIDTH - 200));
            float y = 100.0f + static_cast<float>(rand() % (WINDOW_HEIGHT - 200));

            auto zone = std::make_shared<ReinforcementZone>(Vector2D(x, y));
            zonesToAdd.push_back(zone);

            reinforcementZoneTimer = 0;
        }

        // 添加新区域
        reinforcementZones.insert(reinforcementZones.end(), zonesToAdd.begin(), zonesToAdd.end());
        zonesToAdd.clear();

        // 更新区域
        for (auto it = reinforcementZones.begin(); it != reinforcementZones.end();) {
            (*it)->update(deltaTime);

            if (!(*it)->isActive()) {
                it = reinforcementZones.erase(it);
            }
            else {
                // 处理增援功能：每3秒复制区域内随机飞船
                static float reinforcementTimer = 0;
                reinforcementTimer += deltaTime;

                if (reinforcementTimer >= 3.0f) {
                    reinforceShipsInZone(*it);
                    reinforcementTimer = 0;
                }

                ++it;
            }
        }
    }

    // 在增援区域内复制飞船
    void reinforceShipsInZone(std::shared_ptr<ReinforcementZone> zone) {
        // 收集区域内的飞船
        std::vector<std::shared_ptr<Spaceship>> shipsInZone;

        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            if (obj->getType() == "Spaceship") {
                if (zone->contains(obj->getPosition())) {
                    auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                    if (ship) {
                        shipsInZone.push_back(ship);
                    }
                }
            }
        }

        // 如果区域内有飞船，随机选择一个进行复制
        if (!shipsInZone.empty()) {
            int randomIndex = rand() % shipsInZone.size();
            auto shipToCopy = shipsInZone[randomIndex];

            // 复制飞船（直接创建新飞船，不需要建造时间）
            Vector2D newPos = zone->getRandomPosition();
            sf::Color color = shipToCopy->getBaseColor();
            int faction = shipToCopy->getFactionId();

            auto newShip = std::make_shared<Spaceship>(newPos, color, faction, this,
                shipToCopy->getMass(), shipToCopy->getMaxHealth());

            // 设置随机方向和速度
            float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
            newShip->setRotation(angle);

            float speed = 20.0f + static_cast<float>(rand() % 100) / 100.0f * 30.0f;
            Vector2D velocity(std::cos(angle), std::sin(angle));
            newShip->setVelocity(velocity * speed);

            objectsToAdd.push_back(newShip);
        }
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

        // 移除已收集的光点
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
                // 查找该阵营中血量百分比最高的飞船（且不在建造中）
                std::shared_ptr<Spaceship> bestShip = nullptr;
                float highestHealthRatio = 0;

                for (const auto& obj : objects) {
                    if (!obj->getIsActive()) continue;
                    if (obj->getType() == "Spaceship" && obj->getFactionId() == faction) {
                        auto ship = std::dynamic_pointer_cast<Spaceship>(obj);
                        if (ship && !ship->isCurrentlyBuilding()) {
                            float healthRatio = ship->getHealth() / ship->getMaxHealth();
                            if (healthRatio > highestHealthRatio) {
                                highestHealthRatio = healthRatio;
                                bestShip = ship;
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
            builder->getMass(), builder->getMaxHealth()); // 使用最大血量作为初始血量

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

public:
    PhysicsSystem();

    void addObject(std::shared_ptr<PhysicsObject> obj);
    void addLightPoint(std::shared_ptr<LightPoint> lightPoint);
    void addLaser(const LaserBeam& laser);
    void update(float deltaTime);
    void draw(sf::RenderWindow& window);
    void clear();
    size_t getObjectCount() const;
    size_t getLightPointCount() const;
    std::vector<std::shared_ptr<PhysicsObject>> getAllObjects() const;
    std::vector<std::shared_ptr<LightPoint>> getAllLightPoints() const;
    float getFactionResource(int faction) const;
    int getFactionShipCount(int faction) const;
    void setFactionResource(int faction, float amount);

    // 获取光点列表（供飞船查找使用）
    const std::vector<std::shared_ptr<LightPoint>>& getLightPoints() const { return lightPoints; }

    // 获取阵营资源数组（供飞船建造使用）
    const std::array<float, MAX_FACTIONS>& getFactionResources() const { return factionResources; }
};

// ====================== Spaceship成员函数实现 ======================
Spaceship::Spaceship(Vector2D pos, sf::Color col, int faction, PhysicsSystem* physSys, float m, float h)
    : PhysicsObject(pos, m, h, faction), baseColor(col), size(5.0f),
    fireRate(2.0f), fireTimer(0), fireRange(120.0f), damage(10.0f), fireAngle(PI / 6),
    // 关键修改：降低机动性，增加速度阈值
    maneuverability(0.8f), maxSpeed(120.0f), accelerationRate(50.0f), brakeRate(20.0f),
    currentState(AIState::IDLE), previousState(AIState::IDLE), viewRange(180.0f),
    stateTimer(0), idleActionTimer(0),
    isBuilding(false), buildProgress(0), buildTimer(0), buildSpeed(0.1f), // 新增建造速度
    collectionDistance(10.0f), // 新增收集距离，初始10.0f
    lastDecisionTime(0), decisionInterval(0.5f),
    isTurningIntentionally(false), turningTime(0), turningDuration(0),
    physicsSystem(physSys),
    randomEngine(std::random_device{}()),  // 初始化独立随机引擎
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

    // 初始化视野圆圈
    viewCircle.setRadius(viewRange);
    viewCircle.setFillColor(sf::Color::Transparent);
    viewCircle.setOutlineColor(sf::Color(baseColor.r, baseColor.g, baseColor.b, 100));
    viewCircle.setOutlineThickness(1.0f);
    viewCircle.setOrigin(viewRange, viewRange);

    // 初始化建造环
    buildRing.setRadius(size * 1.5f);
    buildRing.setFillColor(sf::Color::Transparent);
    buildRing.setOutlineColor(sf::Color(255, 255, 255, 200)); // 白色，80%透明度
    buildRing.setOutlineThickness(2.0f);
    buildRing.setOrigin(size * 1.5f, size * 1.5f);
    buildRing.setPointCount(60);

    setRestitution(0.3f);

    // 初始速度 - 给予一个小的随机初始速度
    float initSpeed = 10.0f + randomDist(randomEngine) * 10.0f;
    float initAngle = randomDist(randomEngine) * 2.0f * PI;
    velocity = Vector2D(std::cos(initAngle), std::sin(initAngle)) * initSpeed;

    updateAppearance();
}

void Spaceship::update(float deltaTime) {
    PhysicsObject::update(deltaTime);

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

    // 清理无效的附近光点
    cleanupNearbyLightPoints();

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
        buildProgress = buildTimer * buildSpeed; // 使用建造速度计算进度

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
        if (speedRatio >= 0.9f) {  // 速度达到阈值90%时才回正
            float targetAngle = velocity.angle();
            float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

            // 缓慢转向移动方向
            float maxRotation = maneuverability * deltaTime * 0.3f;
            angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
            rotation += angleDiff;
        }
    }
}

void Spaceship::cleanupNearbyLightPoints() {
    // 清理无效的附近光点
    for (auto it = nearbyLightPoints.begin(); it != nearbyLightPoints.end();) {
        auto lightPoint = it->lock();
        if (!lightPoint || !lightPoint->getIsActive() || lightPoint->isCollectionCompleted()) {
            it = nearbyLightPoints.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Spaceship::collectNearbyLightPoints() {
    if (!physicsSystem) return;

    // 获取所有光点
    auto allLightPoints = physicsSystem->getAllLightPoints();

    for (const auto& lightPoint : allLightPoints) {
        if (!lightPoint->getIsActive()) continue;
        if (lightPoint->isBeingCollected()) continue;
        if (!lightPoint->isCollectible()) continue;

        // 检查距离
        float distance = (lightPoint->getPosition() - position).length();
        if (distance <= collectionDistance) {
            // 立即收集光点
            lightPoint->startCollecting(shared_from_this());

            // 给所属阵营增加资源
            if (physicsSystem) {
                float currentResource = physicsSystem->getFactionResource(factionId);
                physicsSystem->setFactionResource(factionId, currentResource + 1.0f);
            }
        }
    }
}

void Spaceship::updateAI(float deltaTime) {
    // 每0.5秒重新评估状态
    if (lastDecisionTime >= decisionInterval) {
        lastDecisionTime = 0;
        evaluateState();
    }

    // 执行当前状态的行为
    switch (currentState) {
    case AIState::AVOIDING:
        avoidingBehavior(deltaTime);
        break;
    case AIState::ATTACKING:
        attackingBehavior(deltaTime);
        break;
    case AIState::COLLECTING:
        collectingBehavior(deltaTime);
        break;
    case AIState::MINING:
        miningBehavior(deltaTime);
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

    // 状态优先级：避障 > 攻击敌对飞船 > 采集光点 > 攻击小行星 > 空闲

    // 1. 检查是否需要避障
    std::shared_ptr<PhysicsObject> obstacle = findObstacleInPath();
    if (obstacle) {
        currentState = AIState::AVOIDING;
        target = obstacle;
        return;
    }

    // 2. 检查是否有敌对飞船在视野内
    std::shared_ptr<PhysicsObject> enemy = findEnemyInView();
    if (enemy) {
        currentState = AIState::ATTACKING;
        target = enemy;
        return;
    }

    // 3. 检查是否有可收集的光点
    std::shared_ptr<LightPoint> lightPoint = findLightPointInView();
    if (lightPoint) {
        currentState = AIState::COLLECTING;
        target = lightPoint;
        return;
    }

    // 4. 检查是否有小行星在视野内
    std::shared_ptr<PhysicsObject> asteroid = findAsteroidInView();
    if (asteroid) {
        currentState = AIState::MINING;
        target = asteroid;
        return;
    }

    // 5. 空闲状态
    currentState = AIState::IDLE;
    target.reset();
}

void Spaceship::avoidingBehavior(float deltaTime) {
    auto targetObj = target.lock();
    if (!targetObj) {
        currentState = AIState::IDLE;
        return;
    }

    // 计算到障碍物的方向
    Vector2D toObstacle = targetObj->getPosition() - position;
    float distance = toObstacle.length();

    // 如果障碍物太远，退出避障状态
    if (distance > viewRange * 1.5f) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    // 计算避开障碍物的方向（垂直于障碍物方向）
    Vector2D avoidDir = toObstacle.perpendicular().normalized();

    // 根据飞船位置选择避开方向（避免左右摇摆）
    Vector2D shipDir = Vector2D(1, 0).rotated(rotation); // 船头方向
    float dotProduct = avoidDir.dot(shipDir);
    if (dotProduct < 0) {
        avoidDir = -avoidDir;
    }

    // 转向避开方向
    float targetAngle = avoidDir.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    // 根据机动性旋转
    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    // 转弯时适当制动减速
    if (std::abs(angleDiff) > 0.1f) {
        applyBrake(0.3f);
    }

    // 向前推进以避开（转弯时也有前进动力）
    applyThrust(accelerationRate * 0.8f);
}

void Spaceship::attackingBehavior(float deltaTime) {
    auto targetObj = target.lock();
    if (!targetObj || !targetObj->getIsActive() || targetObj->getType() != "Spaceship") {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    Vector2D toTarget = targetObj->getPosition() - position;
    float distance = toTarget.length();

    // 如果目标离开视野，重新评估状态
    if (distance > viewRange) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    // 计算目标方向（用船头对准目标）
    float targetAngle = toTarget.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    // 根据机动性旋转
    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    // 关键修改：攻击时禁止制动，只加速前进
    // 根据距离调整速度
    if (distance > fireRange * 0.7f) {
        // 距离较远，加速接近
        applyThrust(accelerationRate);
    }
    else {
        // 距离适中，保持速度或轻微加速
        if (velocity.length() < maxSpeed * 0.6f) {
            applyThrust(accelerationRate * 0.5f);
        }
    }

    // 尝试攻击
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

    // 检查目标是否是光点
    auto lightPoint = std::dynamic_pointer_cast<LightPoint>(targetObj);
    if (!lightPoint) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    // 检查光点是否已被收集
    if (!lightPoint->getIsActive() || lightPoint->isBeingCollected()) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    // 收集模式AI：制动减速、转向对准、低速靠近
    Vector2D toTarget = lightPoint->getPosition() - position;
    float distance = toTarget.length();

    // 1. 制动减速
    if (distance > collectionDistance * 3.0f) {
        // 距离较远，先减速
        applyBrake(0.5f);
    }

    // 2. 转向对准光点
    float targetAngle = toTarget.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    // 3. 低速靠近光点
    if (distance > collectionDistance * 1.5f) {
        // 距离适中，低速前进
        if (velocity.length() < maxSpeed * 0.3f) {
            applyThrust(accelerationRate * 0.3f);
        }
    }
    else if (distance > collectionDistance) {
        // 接近收集距离，更慢速前进
        if (velocity.length() < maxSpeed * 0.2f) {
            applyThrust(accelerationRate * 0.1f);
        }
    }
    else {
        // 在收集距离内，制动减速
        applyBrake(0.8f);
    }

    // 收集光点（如果进入收集距离）
    if (distance <= collectionDistance) {
        // 立即收集光点
        lightPoint->startCollecting(shared_from_this());

        // 给所属阵营增加资源
        if (physicsSystem) {
            float currentResource = physicsSystem->getFactionResource(factionId);
            physicsSystem->setFactionResource(factionId, currentResource + 1.0f);
        }

        // 重置目标，让AI重新评估
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

    // 如果小行星离开视野，重新评估状态
    if (distance > viewRange) {
        currentState = AIState::IDLE;
        target.reset();
        return;
    }

    // 计算目标方向（用船头对准目标）
    float targetAngle = toTarget.angle();
    float angleDiff = Vector2D::angleDifference(rotation, targetAngle);

    // 根据机动性旋转
    float maxRotation = maneuverability * deltaTime;
    angleDiff = std::clamp(angleDiff, -maxRotation, maxRotation);
    rotation += angleDiff;

    // 保持适当距离
    if (distance > fireRange * 0.8f) {
        applyThrust(accelerationRate * 0.5f);
    }
    else if (distance < fireRange * 0.4f) {
        applyBrake(0.5f);
    }

    // 尝试攻击
    if (std::abs(angleDiff) < fireAngle && distance <= fireRange) {
        tryAttack(targetObj);
    }
}

void Spaceship::idleBehavior(float deltaTime) {
    // 使用独立的随机数生成器

    // 关键修改：大幅增加随机转向的频率和角度
    // 原：每隔1-3秒触发一次，现在改为0.3-1.5秒
    if (idleActionTimer > 0.3f + randomDist(randomEngine) * 1.2f) {
        // 关键修改：增加随机转向角度
        // 原：±45度 * 0.2，现在改为±10度 * 0.8
        float angleChange = (randomDist(randomEngine) - 0.5f) * PI * 0.1f * maneuverability * 0.8f;
        rotation += angleChange;
        isTurningIntentionally = true;
        turningTime = 0;
        turningDuration = 0.5f + randomDist(randomEngine) * 1.0f; // 随机转向持续时间

        // 转弯时制动减速
        if (std::abs(angleChange) > 0.05f && velocity.length() > maxSpeed * 0.3f) {
            applyBrake(0.3f + randomDist(randomEngine) * 0.3f);
        }

        // 关键修改：大幅增加加速概率（80%概率加速）
        if (randomDist(randomEngine) < 0.8f) {
            // 随机加速强度（30%-100%最大加速度）
            float thrust = accelerationRate * (0.3f + randomDist(randomEngine) * 0.7f);
            applyThrust(thrust);
        }
        // 随机决定是否制动（10%概率）
        else if (randomDist(randomEngine) < 0.1f) {
            applyBrake(0.3f + randomDist(randomEngine) * 0.5f);
        }

        idleActionTimer = 0;
    }

    // 确保速度不超过阈值
    if (velocity.length() > maxSpeed * 0.3f) {
        applyBrake(0.2f);
    }

    // 关键修改：即使在等待下一次随机行为时，也有概率进行轻微加速
    if (randomDist(randomEngine) < 0.2f * deltaTime * 60.0f) { // 每帧约12%概率
        applyThrust(accelerationRate * 0.1f);
    }
}

void Spaceship::turningBehavior(float deltaTime) {
    // 主动转向时保持前进动力
    if (velocity.length() < maxSpeed * 0.5f) {
        applyThrust(accelerationRate * 0.3f);
    }
}

void Spaceship::applyThrust(float force) {
    // 推力方向与船头方向一致
    Vector2D thrustDir = Vector2D(1, 0).rotated(rotation); // 船头方向
    Vector2D thrust = thrustDir * force;
    applyForce(thrust);
}

void Spaceship::applyBrake(float intensity) {
    if (velocity.length() < 0.1f) return;

    // 阻力方向与速度方向相反（移动阻力）
    Vector2D brakeDir = -velocity.normalized();
    Vector2D brakeForce = brakeDir * brakeRate * intensity;

    // 应用阻力
    applyForce(brakeForce);
}

std::vector<std::shared_ptr<PixelBlock>> Spaceship::getPixelBlocks() const {
    std::vector<std::shared_ptr<PixelBlock>> blocks;
    float pixelSize = 2.0f;

    // 获取三角形在世界坐标系中的顶点
    auto worldVertices = getConvexHull();

    // 找到三角形的包围盒
    float minX = std::min({ worldVertices[0].x, worldVertices[1].x, worldVertices[2].x });
    float maxX = std::max({ worldVertices[0].x, worldVertices[1].x, worldVertices[2].x });
    float minY = std::min({ worldVertices[0].y, worldVertices[1].y, worldVertices[2].y });
    float maxY = std::max({ worldVertices[0].y, worldVertices[1].y, worldVertices[2].y });

    // 扫描三角形内的像素
    for (float x = minX; x <= maxX; x += pixelSize) {
        for (float y = minY; y <= maxY; y += pixelSize) {
            Vector2D point(x, y);

            // 检查点是否在三角形内（使用重心坐标）
            if (pointInTriangle(point, worldVertices[0], worldVertices[1], worldVertices[2])) {
                // 计算相对于飞船中心的本地坐标
                Vector2D localPos = point - position;
                Vector2D unrotatedPos = localPos.rotated(-rotation);

                // 创建像素块
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

    // 检查目标是否在攻击范围内
    Vector2D toTarget = targetObj->getPosition() - position;
    float distance = toTarget.length();

    if (distance > fireRange) return false;

    // 计算飞船正方向（船头方向）
    Vector2D shipDir = Vector2D(1, 0).rotated(rotation);

    // 计算角度差
    float angleDiff = std::acos(shipDir.dot(toTarget.normalized()));

    if (std::abs(angleDiff) > fireAngle) return false;

    // 发射激光
    Vector2D laserStart = position + shipDir * size;

    // 关键修改：计算激光终点
    Vector2D laserEnd;

    // 根据目标类型调整激光终点
    if (targetObj->getType() == "Asteroid") {
        // 对于小行星，终点在小行星表面（靠近飞船的一侧）
        Vector2D toAsteroid = targetObj->getPosition() - laserStart;
        Vector2D surfacePoint = targetObj->getPosition() - toAsteroid.normalized() * targetObj->getBoundingRadius();
        laserEnd = surfacePoint;
    }
    else {
        // 对于其他目标，直接使用目标位置
        laserEnd = targetObj->getPosition();
    }

    lasers.emplace_back(laserStart, laserEnd, baseColor, shared_from_this(), damage);

    // 将激光添加到物理系统
    if (physicsSystem) {
        physicsSystem->addLaser(lasers.back());
    }

    fireTimer = 0;

    return true;
}

void Spaceship::draw(sf::RenderWindow& window) {
    if (isActive) {
        // 绘制视野圆圈
        viewCircle.setPosition(position.toSFML());
        window.draw(viewCircle);

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
    }
}

void Spaceship::drawBuildRing(sf::RenderWindow& window) {
    buildRing.setPosition(position.toSFML());

    // 根据建造进度设置环的透明度
    int alpha = 50 + static_cast<int>(buildProgress * 150); // 50-200透明度
    sf::Color ringColor = sf::Color(255, 255, 255, static_cast<sf::Uint8>(alpha));
    buildRing.setOutlineColor(ringColor);

    // 绘制环
    window.draw(buildRing);

    // 绘制进度指示器（环形进度条）
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

    // 减少碎片数量：1-2个
    int fragmentCount = 1 + static_cast<int>(randomDist(randomEngine) * 2);

    for (int i = 0; i < fragmentCount; i++) {
        // 随机方向
        float angle = randomDist(randomEngine) * 2.0f * PI;
        float distance = size * (0.5f + randomDist(randomEngine) * 0.5f);

        Vector2D fragmentPos = position + Vector2D(std::cos(angle), std::sin(angle)) * distance;

        // 计算爆炸速度
        Vector2D fragmentVel = velocity + Vector2D(std::cos(angle), std::sin(angle)) *
            (3.0f + randomDist(randomEngine) * 5.0f);

        // 创建小行星碎片（很小）
        float fragmentSize = 6.0f;
        float fragmentMass = mass * 0.05f;
        float fragmentHealth = 10.0f;

        auto fragment = std::make_shared<Asteroid>(fragmentPos, fragmentVel, fragmentSize,
            fragmentMass, fragmentHealth, factionId);
        fragment->setRotation(angle);
        fragment->setAngularVelocity(randomDist(randomEngine) * 2.0f - 1.0f);

        fragments.push_back(fragment);
    }

    return fragments;
}

std::vector<std::shared_ptr<LightPoint>> Spaceship::generateLightPoints() {
    std::vector<std::shared_ptr<LightPoint>> lightPoints;

    // 飞船爆炸生成2-7个光点
    int lightPointCount = 2 + rand() % 6; // 2到7个

    // 如果正在建造，额外生成5-10个光点
    if (isBuilding) {
        lightPointCount += 5 + rand() % 6; // 额外5-10个
    }

    for (int i = 0; i < lightPointCount; i++) {
        // 随机位置
        float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
        float distance = size * (1.0f + static_cast<float>(rand() % 100) / 100.0f);
        Vector2D offset = Vector2D(std::cos(angle), std::sin(angle)) * distance;

        // 随机速度
        Vector2D vel = velocity + offset.normalized() * (2.0f + static_cast<float>(rand() % 100) / 100.0f * 3.0f);

        // 光点颜色使用飞船颜色
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

    return false; // 飞船不会因为出窗口而被删除
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
    if (!physicsSystem) return nullptr;

    // 获取所有光点
    auto allLightPoints = physicsSystem->getAllLightPoints();
    std::shared_ptr<LightPoint> closestLightPoint = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& lightPoint : allLightPoints) {
        if (!lightPoint->getIsActive()) continue;
        if (lightPoint->isBeingCollected()) continue; // 跳过正在被收集的光点
        if (!lightPoint->isCollectible()) continue;   // 跳过已被锁定的光点

        float distance = (lightPoint->getPosition() - position).length();
        if (distance <= viewRange && distance < minDistance) {
            minDistance = distance;
            closestLightPoint = lightPoint;
        }
    }

    return closestLightPoint;
}

void Spaceship::updateAppearance() {
    // 计算血量比例
    float healthRatio = health / maxHealth;

    // 根据血量调整颜色亮度
    sf::Color shipColor = baseColor;
    sf::Color glow = baseColor;

    // 血量越低颜色越暗，发光越弱
    float brightness = 0.3f + 0.7f * healthRatio;
    shipColor.r = static_cast<sf::Uint8>(shipColor.r * brightness);
    shipColor.g = static_cast<sf::Uint8>(shipColor.g * brightness);
    shipColor.b = static_cast<sf::Uint8>(shipColor.b * brightness);

    // 发光颜色（更亮，半透明）
    glow.a = static_cast<sf::Uint8>(150 * healthRatio);
    glow.r = std::min(255, static_cast<int>(glow.r * (0.8f + 0.5f * healthRatio)));
    glow.g = std::min(255, static_cast<int>(glow.g * (0.8f + 0.5f * healthRatio)));
    glow.b = std::min(255, static_cast<int>(glow.b * (0.8f + 0.5f * healthRatio)));

    // 更新飞船形状
    shape.setFillColor(shipColor);
    for (int i = 0; i < 3; i++) {
        Vector2D rotated = localVertices[i].rotated(rotation);
        shape.setPoint(i, sf::Vector2f(position.x + rotated.x, position.y + rotated.y));
    }

    // 更新发光形状（比飞船大30%）
    glowShape.setFillColor(glow);
    float glowScale = 1.3f;
    for (int i = 0; i < 3; i++) {
        Vector2D rotated = localVertices[i].rotated(rotation) * glowScale;
        glowShape.setPoint(i, sf::Vector2f(position.x + rotated.x, position.y + rotated.y));
    }

    // 更新视野圆圈颜色（根据血量调整透明度）
    sf::Color viewColor = baseColor;
    viewColor.a = static_cast<sf::Uint8>(50 * healthRatio);
    viewCircle.setOutlineColor(viewColor);
}

std::shared_ptr<PhysicsObject> Spaceship::findObstacleInPath() {
    if (!physicsSystem) return nullptr;

    Vector2D shipDir = Vector2D(1, 0).rotated(rotation);
    float checkDistance = 100.0f;
    Vector2D checkPoint = position + shipDir * checkDistance;

    auto allObjects = physicsSystem->getAllObjects();
    std::shared_ptr<PhysicsObject> closestObstacle = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue; // 跳过自己
        if (obj->getType() == "Asteroid") {
            Vector2D toObj = obj->getPosition() - position;
            float distance = toObj.length();
            float dotProduct = shipDir.dot(toObj.normalized());

            // 检查是否在飞船前方且距离较近
            if (dotProduct > 0.8f && distance < checkDistance && distance < minDistance) {
                minDistance = distance;
                closestObstacle = obj;
            }
        }
    }

    return closestObstacle;
}

std::shared_ptr<PhysicsObject> Spaceship::findEnemyInView() {
    if (!physicsSystem) return nullptr;

    auto allObjects = physicsSystem->getAllObjects();
    std::shared_ptr<PhysicsObject> closestEnemy = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue; // 跳过自己
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
    if (!physicsSystem) return nullptr;

    auto allObjects = physicsSystem->getAllObjects();
    std::shared_ptr<PhysicsObject> closestAsteroid = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& obj : allObjects) {
        if (!obj->getIsActive()) continue;
        if (obj.get() == this) continue; // 跳过自己
        if (obj->getType() == "Asteroid") {
            // 检查是否在视野内
            float distance = (obj->getPosition() - position).length();
            if (distance <= viewRange && distance < minDistance) {
                minDistance = distance;
                closestAsteroid = obj;
            }
        }
    }

    return closestAsteroid;
}

// ====================== PhysicsSystem成员函数实现 ======================
PhysicsSystem::PhysicsSystem() : reinforcementZoneTimer(0) {
    spatialGrid.resize(GRID_HEIGHT, std::vector<std::vector<std::shared_ptr<PhysicsObject>>>(GRID_WIDTH));
    factionResources.fill(0.0f);
    factionShipCounts.fill(0);
}

void PhysicsSystem::addObject(std::shared_ptr<PhysicsObject> obj) {
    objectsToAdd.push_back(obj);
}

void PhysicsSystem::addLightPoint(std::shared_ptr<LightPoint> lightPoint) {
    lightPointsToAdd.push_back(lightPoint);
    // 光点也是物理对象，添加到objects列表
    objectsToAdd.push_back(lightPoint);
}

void PhysicsSystem::addLaser(const LaserBeam& laser) {
    lasers.push_back(laser);
}

void PhysicsSystem::update(float deltaTime) {
    // 添加新物体
    objects.insert(objects.end(), objectsToAdd.begin(), objectsToAdd.end());
    objectsToAdd.clear();

    // 添加新光点
    lightPoints.insert(lightPoints.end(), lightPointsToAdd.begin(), lightPointsToAdd.end());
    lightPointsToAdd.clear();

    // 移除飞出生成区的物体
    objects.erase(std::remove_if(objects.begin(), objects.end(),
        [](const std::shared_ptr<PhysicsObject>& obj) {
            if (!obj->getIsActive()) return true;
            return obj->isOutOfBounds();
        }),
        objects.end());

    // 移除飞出生成区的光点
    lightPoints.erase(std::remove_if(lightPoints.begin(), lightPoints.end(),
        [](const std::shared_ptr<LightPoint>& lp) {
            if (!lp->getIsActive()) return true;
            return lp->isOutOfBounds();
        }),
        lightPoints.end());

    // 更新空间分区
    updateSpatialGrid();

    // 更新所有物体
    for (auto& obj : objects) {
        if (obj->getIsActive()) {
            obj->update(deltaTime);
        }
    }

    // 处理激光
    processLasers(deltaTime);

    // 处理增援区域
    processReinforcementZones(deltaTime);

    // 处理光点收集 - 简化版本
    processLightPointCollection();

    // 处理飞船建造
    processShipBuilding();

    // 碰撞检测与响应
    std::vector<std::pair<std::shared_ptr<PhysicsObject>, std::shared_ptr<PhysicsObject>>> collisions;

    for (size_t i = 0; i < objects.size(); i++) {
        if (!objects[i]->getIsActive()) continue;

        auto nearby = getNearbyObjects(objects[i]);

        for (auto& obj2 : nearby) {
            if (obj2 == objects[i] || !obj2->getIsActive()) continue;

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

void PhysicsSystem::draw(sf::RenderWindow& window) {
    // 绘制光点
    for (auto& lp : lightPoints) {
        lp->draw(window);
    }

    // 绘制激光
    for (auto& laser : lasers) {
        laser.draw(window);
    }

    // 绘制增援区域
    for (auto& zone : reinforcementZones) {
        zone->draw(window);
    }

    // 绘制物体
    for (auto& obj : objects) {
        if (obj->getIsActive()) {
            obj->draw(window);
        }
    }
}

void PhysicsSystem::clear() {
    objects.clear();
    lightPoints.clear();
    objectsToAdd.clear();
    lightPointsToAdd.clear();
    lasers.clear();
    reinforcementZones.clear();
    zonesToAdd.clear();
    factionResources.fill(0.0f);
    factionShipCounts.fill(0);
    reinforcementZoneTimer = 0;
}

size_t PhysicsSystem::getObjectCount() const {
    return objects.size();
}

size_t PhysicsSystem::getLightPointCount() const {
    return lightPoints.size();
}

std::vector<std::shared_ptr<PhysicsObject>> PhysicsSystem::getAllObjects() const {
    return objects;
}

std::vector<std::shared_ptr<LightPoint>> PhysicsSystem::getAllLightPoints() const {
    return lightPoints;
}

float PhysicsSystem::getFactionResource(int faction) const {
    if (faction >= 0 && faction < MAX_FACTIONS) {
        return factionResources[faction];
    }
    return 0.0f;
}

int PhysicsSystem::getFactionShipCount(int faction) const {
    if (faction >= 0 && faction < MAX_FACTIONS) {
        return factionShipCounts[faction];
    }
    return 0;
}

void PhysicsSystem::setFactionResource(int faction, float amount) {
    if (faction >= 0 && faction < MAX_FACTIONS) {
        factionResources[faction] = amount;
    }
}

// ====================== UI系统 ======================
class UISystem {
private:
    GameFont font;
    bool showUI;
    std::vector<sf::Color> factionColors;
    std::vector<sf::String> factionNames;  // 使用sf::String支持Unicode
    PhysicsSystem* physicsSystem;

public:
    UISystem(PhysicsSystem* physSys) : showUI(false), physicsSystem(physSys) {
        // 初始化阵营颜色
        factionColors = {
            sf::Color(255, 50, 50),      // 红色
            sf::Color(50, 150, 255),     // 蓝色
            sf::Color(50, 255, 100),     // 绿色
            sf::Color(255, 255, 50)      // 黄色
        };

        // 使用sf::String直接存储Unicode字符串
        factionNames = {
            L"红色阵营",  // 红色阵营
            L"蓝色阵营",  // 蓝色阵营
            L"绿色阵营",  // 绿色阵营
            L"黄色阵营"   // 黄色阵营
        };
    }

    // 切换UI显示状态
    void toggleUI() {
        showUI = !showUI;
    }

    // 设置UI显示状态
    void setUIVisible(bool visible) {
        showUI = visible;
    }

    // 绘制UI
    void draw(sf::RenderWindow& window) {
        if (!showUI) return;

        // 获取字体
        const sf::Font& uiFont = font.getFont();
        if (!font.isLoaded()) return;

        // 在四个角显示阵营信息
        std::vector<sf::Vector2f> corners = {
            sf::Vector2f(20, 20),                         // 左上角
            sf::Vector2f(WINDOW_WIDTH - 200, 20),         // 右上角
            sf::Vector2f(20, WINDOW_HEIGHT - 100),        // 左下角
            sf::Vector2f(WINDOW_WIDTH - 200, WINDOW_HEIGHT - 100) // 右下角
        };

        for (int faction = 0; faction < 4; faction++) {
            drawFactionInfo(window, uiFont, faction, corners[faction]);
        }
    }

private:
    // 绘制单个阵营信息
    void drawFactionInfo(sf::RenderWindow& window, const sf::Font& font, int faction, sf::Vector2f position) {
        // 获取阵营数据
        float resources = physicsSystem->getFactionResource(faction);
        int ships = physicsSystem->getFactionShipCount(faction);

        // 创建背景矩形
        sf::RectangleShape background(sf::Vector2f(180, 80));
        background.setPosition(position);
        background.setFillColor(sf::Color(0, 0, 0, 150));
        background.setOutlineColor(factionColors[faction]);
        background.setOutlineThickness(2);
        window.draw(background);

        // 绘制阵营颜色圆圈
        sf::CircleShape colorCircle(8);
        colorCircle.setPosition(position.x + 10, position.y + 15);
        colorCircle.setFillColor(factionColors[faction]);
        window.draw(colorCircle);

        // 绘制阵营名称（使用sf::String确保Unicode正确显示）
        sf::Text nameText;
        nameText.setFont(font);
        nameText.setString(factionNames[faction]);
        nameText.setCharacterSize(16);
        nameText.setFillColor(sf::Color::White);
        nameText.setPosition(position.x + 30, position.y + 10);
        window.draw(nameText);

        // 绘制飞船数量
        sf::Text shipText;
        shipText.setFont(font);
        shipText.setString(L"飞船: " + std::to_wstring(ships));
        shipText.setCharacterSize(14);
        shipText.setFillColor(sf::Color::White);
        shipText.setPosition(position.x + 30, position.y + 35);
        window.draw(shipText);

        // 绘制资源数量
        sf::Text resourceText;
        resourceText.setFont(font);
        resourceText.setString(L"资源: " + std::to_wstring(static_cast<int>(resources)));
        resourceText.setCharacterSize(14);
        resourceText.setFillColor(sf::Color::White);
        resourceText.setPosition(position.x + 30, position.y + 55);
        window.draw(resourceText);
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

    // 随机数生成
    std::mt19937 rng;
    std::uniform_real_distribution<float> distPos;
    std::uniform_real_distribution<float> distVel;
    std::uniform_real_distribution<float> distSize;

    // 小行星生成
    float asteroidTimer;
    float timeSinceLastWave;
    int waveCount;

public:
    Game(int width, int height)
        : window(sf::VideoMode(width, height), L"暗域光战", sf::Style::Close),
        physicsSystem(),
        uiSystem(&physicsSystem),
        rng(std::random_device()()),
        distPos(0, 1),
        distVel(5, 15),  // 降低速度范围
        distSize(15, 35),
        asteroidTimer(0),
        timeSinceLastWave(0),
        waveCount(0) {

        // 开启抗锯齿
        sf::ContextSettings settings;
        settings.antialiasingLevel = 8;
        window.create(sf::VideoMode(width, height), L"暗域光战", sf::Style::Close, settings);
        window.setFramerateLimit(60);

        // 初始化飞船
        initializeShips();
    }

    void initializeShips() {
        float margin = 80.0f;

        // 每个阵营5条飞船，单排斜放
        std::vector<sf::Color> colors = {
            sf::Color(255, 50, 50),      // 红色
            sf::Color(50, 150, 255),     // 蓝色
            sf::Color(50, 255, 100),     // 绿色
            sf::Color(255, 255, 50)      // 黄色
        };

        // 为每个阵营创建5艘飞船
        for (int faction = 0; faction < 4; faction++) {
            // 根据阵营确定起始位置
            Vector2D startPos;
            Vector2D direction;

            switch (faction) {
            case 0: // 红色 - 左上角
                startPos = Vector2D(margin, margin);
                direction = Vector2D(1, 1).normalized();
                break;
            case 1: // 蓝色 - 右上角
                startPos = Vector2D(WINDOW_WIDTH - margin, margin);
                direction = Vector2D(-1, 1).normalized();
                break;
            case 2: // 绿色 - 左下角
                startPos = Vector2D(margin, WINDOW_HEIGHT - margin);
                direction = Vector2D(1, -1).normalized();
                break;
            case 3: // 黄色 - 右下角
                startPos = Vector2D(WINDOW_WIDTH - margin, WINDOW_HEIGHT - margin);
                direction = Vector2D(-1, -1).normalized();
                break;
            }

            // 创建5艘飞船，斜向排列
            for (int i = 0; i < 5; i++) {
                // 计算位置偏移
                Vector2D offset = direction * (i * 16.0f);
                Vector2D shipPos = startPos + offset;

                // 创建飞船
                auto ship = std::make_shared<Spaceship>(shipPos, colors[faction], faction, &physicsSystem);

                // 朝向窗口中心
                Vector2D center(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
                Vector2D toCenter = center - ship->getPosition();
                ship->setRotation(toCenter.angle());

                ship->setAngularVelocity(0);

                // 初始速度朝向中心
                Vector2D initialVel = toCenter.normalized() * (15.0f + (i * 2.0f)); // 每艘飞船速度略微增加
                ship->setVelocity(initialVel);

                physicsSystem.addObject(ship);
            }
        }
    }

    // 生成小行星波次
    void generateAsteroidWave() {
        // 恢复小行星数量到合理水平
        float baseCount = 2.0f;  // 基础数量
        float timeFactor = std::min(5.0f, timeSinceLastWave / 60.0f); // 每60秒增加一个
        int asteroidCount = static_cast<int>(baseCount + timeFactor);

        // 偶尔（2%概率）生成较多小行星
        if (rand() % 100 < 2) {
            asteroidCount += 2 + rand() % 3;
        }

        // 限制最大数量
        asteroidCount = std::min(asteroidCount, 6);

        for (int i = 0; i < asteroidCount; i++) {
            addRandomAsteroid();
        }

        timeSinceLastWave = 0;
        waveCount++;
    }

    void addRandomAsteroid() {
        // 随机从生成区边缘进入
        Vector2D position;
        Vector2D velocity;

        int entrySide = rand() % 4;

        // 主要方向（96%概率在同一方向，偏差30度以内）
        float baseAngle;
        switch (entrySide) {
        case 0: // 左边
            position = Vector2D(-50, distPos(rng) * SPAWN_AREA_HEIGHT);
            baseAngle = 0; // 向右
            break;
        case 1: // 右边
            position = Vector2D(SPAWN_AREA_WIDTH + 50, distPos(rng) * SPAWN_AREA_HEIGHT);
            baseAngle = PI; // 向左
            break;
        case 2: // 上边
            position = Vector2D(distPos(rng) * SPAWN_AREA_WIDTH, -50);
            baseAngle = PI / 2; // 向下
            break;
        case 3: // 下边
            position = Vector2D(distPos(rng) * SPAWN_AREA_WIDTH, SPAWN_AREA_HEIGHT + 50);
            baseAngle = -PI / 2; // 向上
            break;
        }

        // 96%概率方向偏差在30度以内，4%概率随机方向
        float angleVariation;
        if (rand() % 100 < 96) {
            angleVariation = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * PI / 6; // ±30度
        }
        else {
            angleVariation = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * PI; // 随机方向
        }

        float speed = distVel(rng);
        velocity = Vector2D(std::cos(baseAngle + angleVariation), std::sin(baseAngle + angleVariation)) * speed;

        // 随机大小
        float size;
        if (rand() % 100 < 70) {  // 70%中等大小
            size = 20.0f + distSize(rng) * 0.5f;
            velocity = velocity * 0.7f;
        }
        else if (rand() % 100 < 85) {  // 15%大型低速
            size = 30.0f + distSize(rng);
            velocity = velocity * 0.4f;
        }
        else {  // 15%小型高速
            size = 10.0f + distSize(rng) * 0.3f;
            velocity = velocity * 1.5f;
        }

        float mass = size * 2.0f;

        auto asteroid = std::make_shared<Asteroid>(position, velocity, size, mass);

        // 添加随机自旋
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
                    // 按Esc键切换UI显示
                    uiSystem.toggleUI();
                }

                // R键重置
                if (event.key.code == sf::Keyboard::R) {
                    physicsSystem.clear();
                    initializeShips();
                    asteroidTimer = 0;
                    timeSinceLastWave = 0;
                    waveCount = 0;
                }

                // 空格键手动生成小行星波次
                if (event.key.code == sf::Keyboard::Space) {
                    generateAsteroidWave();
                }
            }
        }
    }

    void update() {
        float deltaTime = clock.restart().asSeconds();

        // 限制deltaTime，防止卡顿导致物理不稳定
        deltaTime = std::min(deltaTime, 0.1f);

        // 更新游戏时间
        timeSinceLastWave += deltaTime;

        // 更新物理系统
        physicsSystem.update(deltaTime);

        // 生成小行星波次
        asteroidTimer += deltaTime;

        // 调整波次间隔：基础15秒，随着时间增加逐渐缩短到8秒
        float baseInterval = 15.0f;
        float minInterval = 8.0f;
        float interval = std::max(minInterval, baseInterval - static_cast<float>(waveCount) * 0.1f);

        if (asteroidTimer >= interval) {
            generateAsteroidWave();
            asteroidTimer = 0;
        }
    }

    void render() {
        window.clear(sf::Color::Black);

        // 绘制所有物理对象和光点
        physicsSystem.draw(window);

        // 绘制UI
        uiSystem.draw(window);

        window.display();
    }
};

// ====================== 主函数 ======================
int main() {
    // 设置随机种子
    srand(static_cast<unsigned int>(time(nullptr)));

    // ====================== 彻底隐藏控制台 ======================
    // 使用两种方法确保控制台窗口完全隐藏：
    // 1. 链接器选项（已在文件开头设置）：/subsystem:windows /ENTRY:mainCRTStartup
    // 2. Windows API调用：FreeConsole() - 这会完全释放控制台
#ifdef _WIN32
    // 先尝试获取控制台句柄
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        // 隐藏控制台窗口
        ShowWindow(consoleWnd, SW_HIDE);
        // 然后释放控制台
        FreeConsole();
    }
#endif

    // 创建游戏实例
    Game game(WINDOW_WIDTH, WINDOW_HEIGHT);
    game.run();

    return 0;
}