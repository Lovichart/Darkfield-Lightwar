// 
// 游戏名 暗域光战 - Darkfield Lightwar
// 
// 开发环境配置：
// 1. Visual Studio 2022
// 2. SFML 2.6.2 库
// 3. C++17 标准
// 4. 字符编码：UTF-8 with BOM
//

// 彻底隐藏控制台窗口
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

// ====================== 物理常量 ======================
constexpr float PI = 3.14159265358979323846f;
constexpr float TIME_STEP = 1.0f / 60.0f;    // 固定时间步长，确保物理稳定

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

// 用于凸包算法的点比较
struct PointCompare {
    bool operator()(const Vector2D& a, const Vector2D& b) const {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    }
};

// ====================== 前向声明 ======================
class Asteroid;  // 前向声明

// ====================== 激光类 ======================
class LaserBeam {
private:
    Vector2D startPos;
    Vector2D endPos;
    sf::Color color;
    float lifeTime;
    float maxLifeTime;
    float thickness;
    
public:
    LaserBeam(Vector2D start, Vector2D end, sf::Color col, float duration = 0.1f)
        : startPos(start), endPos(end), color(col), maxLifeTime(duration), thickness(1.5f) {
        lifeTime = maxLifeTime;
        
        // 增强激光效果
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
        
        // 绘制光晕效果（多层不同颜色的线）
        sf::Color glowColor = color;
        glowColor.a = 100;
        
        // 使用三角形条带模拟更宽的线
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
    }
    
    bool isAlive() const { return lifeTime > 0; }
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
        shape.setOrigin(size/2, size/2);
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
        sf::Vector2f worldPos(parentPos.x + rotatedPos.x - size/2, 
                             parentPos.y + rotatedPos.y - size/2);
        return sf::FloatRect(worldPos, sf::Vector2f(size, size));
    }
    
    // 检查点是否在像素块内
    bool contains(Vector2D worldPoint, Vector2D parentPos, float parentRotation = 0) const {
        Vector2D rotatedPos = localPosition.rotated(parentRotation);
        Vector2D blockCenter = parentPos + rotatedPos;
        Vector2D delta = worldPoint - blockCenter;
        return std::abs(delta.x) <= size/2 && std::abs(delta.y) <= size/2;
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
class PhysicsObject {
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
    
public:
    PhysicsObject(Vector2D pos, float m, float h)
        : position(pos), velocity(0, 0), acceleration(0, 0),
          mass(m), health(h), maxHealth(h), restitution(0.2f), 
          isActive(true), rotation(0), angularVelocity(0) {}
    
    virtual ~PhysicsObject() = default;
    
    // 更新物理状态
    virtual void update(float deltaTime) {
        if (!isActive) return;
        
        // 无阻力太空物理
        velocity = velocity + acceleration * deltaTime;
        position = position + velocity * deltaTime;
        rotation += angularVelocity * deltaTime;
        
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
    virtual void takeDamage(float damage, Vector2D damagePoint = Vector2D(0, 0)) {
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
    
    // 绘制函数
    virtual void draw(sf::RenderWindow& window) = 0;
    
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
    void setVelocity(const Vector2D& v) { velocity = v; }
    void addVelocity(const Vector2D& v) { velocity = velocity + v; }
    void setPosition(const Vector2D& p) { position = p; }
    void setRestitution(float r) { restitution = std::min(std::max(r, 0.0f), 1.0f); }
    void setRotation(float r) { rotation = r; }
    void setAngularVelocity(float av) { angularVelocity = av; }
    
    // 检查是否飞出屏幕
    virtual bool isOutOfBounds(float screenWidth, float screenHeight) {
        float margin = 100.0f;
        return (position.x < -margin || position.x > screenWidth + margin ||
                position.y < -margin || position.y > screenHeight + margin);
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
    Asteroid(Vector2D pos, Vector2D vel, float radius, float m = 50.0f, float h = 200.0f)
        : PhysicsObject(pos, m, h), pixelSize(4.0f), canSplit(true), hullDirty(true) {
        
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
        return pixelBlocks;
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
                cachedConvexHull = {position};
            } else if (points.size() == 1) {
                cachedConvexHull = {points[0]};
            } else {
                // 两个点的情况
                cachedConvexHull = {points[0], points[1]};
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
                } else {
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
    
    void takeDamage(float damage, Vector2D damagePoint = Vector2D(0, 0)) override {
        if (!isActive) return;
        
        PhysicsObject::takeDamage(damage);
        
        // 如果伤害点接近原点，使用随机边缘点
        if (damagePoint.length() < 1.0f) {
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
                float distance = std::sqrt(static_cast<float>(x*x + y*y));
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
        
        // 确保有足够像素块 - 修复无限递归
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
                    float distance = std::sqrt(static_cast<float>(x*x + y*y));
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
                    } else {
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
            if (pixel->isActive()) {
                sortedPixels.push_back(pixel);
            }
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
    float range;             // 射程
    float damage;            // 单发伤害
    float fireAngle;         // 射击角度（弧度）
    std::vector<LaserBeam> lasers;
    
public:
    Spaceship(Vector2D pos, sf::Color col, float m = 3.0f, float h = 100.0f)
        : PhysicsObject(pos, m, h), baseColor(col), size(7.0f), 
          fireRate(2.0f), fireTimer(0), range(300.0f), damage(25.0f), fireAngle(PI / 6) {
        
        // 创建等腰三角形（船头尖） - 原来大小的1/3
        localVertices.resize(3);
        localVertices[0] = Vector2D(0, -size);          // 顶部（船头）
        localVertices[1] = Vector2D(-size * 0.7f, size * 0.8f);  // 左后
        localVertices[2] = Vector2D(size * 0.7f, size * 0.8f);   // 右后
        
        // 初始化飞船形状
        shape.setPointCount(3);
        shape.setOutlineThickness(0);
        
        // 初始化发光形状（比飞船稍大）
        glowShape.setPointCount(3);
        glowShape.setOutlineThickness(0);
        
        setRestitution(0.3f);
        
        updateAppearance();
    }
    
    void update(float deltaTime) override {
        PhysicsObject::update(deltaTime);
        updateAppearance();
        
        // 更新射击计时器
        fireTimer += deltaTime;
        
        // 更新激光
        for (auto it = lasers.begin(); it != lasers.end();) {
            it->update(deltaTime);
            if (!it->isAlive()) {
                it = lasers.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // 获取精确碰撞像素块 - 为三角形生成近似像素表示
    std::vector<std::shared_ptr<PixelBlock>> getPixelBlocks() const override {
        std::vector<std::shared_ptr<PixelBlock>> blocks;
        float pixelSize = 2.0f; // 比小行星像素更细
        
        // 获取三角形在世界坐标系中的顶点
        auto worldVertices = getConvexHull();
        
        // 找到三角形的包围盒
        float minX = std::min({worldVertices[0].x, worldVertices[1].x, worldVertices[2].x});
        float maxX = std::max({worldVertices[0].x, worldVertices[1].x, worldVertices[2].x});
        float minY = std::min({worldVertices[0].y, worldVertices[1].y, worldVertices[2].y});
        float maxY = std::max({worldVertices[0].y, worldVertices[1].y, worldVertices[2].y});
        
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
    
    // 获取凸包顶点（就是三角形的三个顶点）
    std::vector<Vector2D> getConvexHull() const override {
        std::vector<Vector2D> worldVertices;
        for (const auto& vertex : localVertices) {
            Vector2D rotated = vertex.rotated(rotation);
            worldVertices.push_back(position + rotated);
        }
        return worldVertices;
    }
    
    // 获取外接圆半径
    float getBoundingRadius() const override {
        float maxDist = 0;
        for (const auto& vertex : localVertices) {
            float dist = vertex.length();
            if (dist > maxDist) maxDist = dist;
        }
        return maxDist;
    }
    
    // 尝试攻击目标
    bool tryAttack(std::shared_ptr<PhysicsObject> target) {
        if (fireTimer < 1.0f / fireRate) return false;
        
        // 计算到目标的向量
        Vector2D toTarget = target->getPosition() - position;
        float distance = toTarget.length();
        
        if (distance > range) return false;
        
        // 计算飞船正方向（船头方向）
        Vector2D shipDir = Vector2D(0, -1).rotated(rotation);
        
        // 计算角度差
        float angleDiff = std::acos(shipDir.dot(toTarget.normalized()));
        
        if (std::abs(angleDiff) > fireAngle) return false;
        
        // 发射激光
        Vector2D laserStart = position + shipDir * size;
        
        // 计算激光结束点（打到目标边缘）
        float targetRadius = target->getBoundingRadius();
        Vector2D laserEnd = target->getPosition() - toTarget.normalized() * targetRadius * 0.8f;
        
        lasers.emplace_back(laserStart, laserEnd, baseColor);
        fireTimer = 0;
        
        // 对目标造成伤害
        target->takeDamage(damage, laserEnd);
        
        return true;
    }
    
    void draw(sf::RenderWindow& window) override {
        if (isActive) {
            // 绘制激光
            for (const auto& laser : lasers) {
                laser.draw(window);
            }
            
            // 绘制发光效果
            window.draw(glowShape);
            
            // 绘制飞船主体
            window.draw(shape);
        }
    }
    
    // 飞船爆炸效果
    std::vector<std::shared_ptr<PhysicsObject>> onDestroy() override {
        std::vector<std::shared_ptr<PhysicsObject>> fragments;
        
        // 飞船爆炸生成三角形碎片
        int fragmentCount = 8 + rand() % 8;
        for (int i = 0; i < fragmentCount; i++) {
            // 计算碎片大小（逐渐变小）
            float fragmentSize = size * (0.3f + 0.2f * static_cast<float>(rand() % 100) / 100.0f);
            
            // 确保碎片大小至少为像素大小
            fragmentSize = std::max(fragmentSize, 4.0f);
            
            // 随机方向
            float angle = static_cast<float>(rand() % 100) / 100.0f * 2.0f * PI;
            float distance = size * (0.5f + static_cast<float>(rand() % 100) / 100.0f);
            
            Vector2D fragmentPos = position + Vector2D(std::cos(angle), std::sin(angle)) * distance;
            
            // 计算爆炸速度（径向速度加上随机性）
            Vector2D fragmentVel = velocity + Vector2D(std::cos(angle), std::sin(angle)) * 
                                  (5.0f + static_cast<float>(rand() % 100) / 10.0f);
            
            // 创建小岩石碎片
            float fragmentMass = mass * 0.1f;
            float fragmentHealth = 5.0f; // 小碎片血量很低
            
            // 创建小行星碎片（使用Asteroid类的最小版本）
            auto fragment = std::make_shared<Asteroid>(fragmentPos, fragmentVel, fragmentSize, 
                                                      fragmentMass, fragmentHealth);
            fragment->setRotation(angle);
            fragment->setAngularVelocity(static_cast<float>(rand() % 100) / 100.0f * 2.0f - 1.0f);
            
            fragments.push_back(fragment);
        }
        
        return fragments;
    }
    
    // 限制飞船在屏幕内
    bool isOutOfBounds(float screenWidth, float screenHeight) override {
        float margin = getBoundingRadius();
        
        if (position.x < margin) {
            position.x = margin;
            velocity.x = std::abs(velocity.x) * 0.5f; // 轻微反弹
            return false;
        }
        if (position.x > screenWidth - margin) {
            position.x = screenWidth - margin;
            velocity.x = -std::abs(velocity.x) * 0.5f;
            return false;
        }
        if (position.y < margin) {
            position.y = margin;
            velocity.y = std::abs(velocity.y) * 0.5f;
            return false;
        }
        if (position.y > screenHeight - margin) {
            position.y = screenHeight - margin;
            velocity.y = -std::abs(velocity.y) * 0.5f;
            return false;
        }
        
        return false;
    }
    
private:
    // 检查点是否在三角形内（重心坐标法）
    bool pointInTriangle(Vector2D p, Vector2D a, Vector2D b, Vector2D c) const {
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
    
    void updateAppearance() {
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
    }
};

// ====================== 精确碰撞检测系统 ======================
class PhysicsSystem {
private:
    std::vector<std::shared_ptr<PhysicsObject>> objects;
    std::vector<std::shared_ptr<PhysicsObject>> objectsToAdd;
    
    // 空间分区网格
    static const int GRID_SIZE = 100;
    static const int GRID_WIDTH = 13;  // 1280/100 ≈ 13
    static const int GRID_HEIGHT = 8;  // 720/100 ≈ 8
    std::vector<std::vector<std::shared_ptr<PhysicsObject>>> spatialGrid;
    
    // 精确像素碰撞检测
    bool checkPixelCollision(std::shared_ptr<PhysicsObject> a, std::shared_ptr<PhysicsObject> b) {
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
    Vector2D checkLaserCollision(Vector2D start, Vector2D end, std::shared_ptr<PhysicsObject> target) {
        // 获取目标的凸包
        auto hull = target->getConvexHull();
        if (hull.size() < 3) return end;
        
        // 检查激光线段与凸包每条边的交点
        Vector2D intersection = end;
        float minDist = std::numeric_limits<float>::max();
        
        for (size_t i = 0; i < hull.size(); i++) {
            Vector2D p1 = hull[i];
            Vector2D p2 = hull[(i + 1) % hull.size()];
            
            Vector2D intersect = lineSegmentIntersection(start, end, p1, p2);
            if (intersect.x != -1) {
                float dist = (intersect - start).lengthSquared();
                if (dist < minDist) {
                    minDist = dist;
                    intersection = intersect;
                }
            }
        }
        
        return intersection;
    }
    
    Vector2D lineSegmentIntersection(Vector2D a1, Vector2D a2, Vector2D b1, Vector2D b2) {
        Vector2D s1 = a2 - a1;
        Vector2D s2 = b2 - b1;
        
        float s, t;
        s = (-s1.y * (a1.x - b1.x) + s1.x * (a1.y - b1.y)) / (-s2.x * s1.y + s1.x * s2.y);
        t = ( s2.x * (a1.y - b1.y) - s2.y * (a1.x - b1.x)) / (-s2.x * s1.y + s1.x * s2.y);
        
        if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
            return Vector2D(a1.x + (t * s1.x), a1.y + (t * s1.y));
        }
        
        return Vector2D(-1, -1); // 没有交点
    }
    
    // 计算碰撞响应
    void resolveCollision(std::shared_ptr<PhysicsObject> a, std::shared_ptr<PhysicsObject> b,
                         Vector2D collisionPoint, Vector2D normal) {
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
                cell = nullptr;
            }
        }
        
        // 将物体放入网格
        for (const auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            
            Vector2D pos = obj->getPosition();
            int gridX = std::min(GRID_WIDTH - 1, std::max(0, static_cast<int>(pos.x / GRID_SIZE)));
            int gridY = std::min(GRID_HEIGHT - 1, std::max(0, static_cast<int>(pos.y / GRID_SIZE)));
            
            spatialGrid[gridY][gridX] = obj;
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
                    if (spatialGrid[y][x] && spatialGrid[y][x] != obj) {
                        nearby.push_back(spatialGrid[y][x]);
                    }
                }
            }
        }
        
        return nearby;
    }
    
public:
    PhysicsSystem() {
        spatialGrid.resize(GRID_HEIGHT, std::vector<std::shared_ptr<PhysicsObject>>(GRID_WIDTH, nullptr));
    }
    
    void addObject(std::shared_ptr<PhysicsObject> obj) {
        objectsToAdd.push_back(obj);
    }
    
    void update(float deltaTime, float screenWidth, float screenHeight) {
        // 添加新物体
        objects.insert(objects.end(), objectsToAdd.begin(), objectsToAdd.end());
        objectsToAdd.clear();
        
        // 移除飞出屏幕的小行星
        objects.erase(std::remove_if(objects.begin(), objects.end(),
            [screenWidth, screenHeight](const std::shared_ptr<PhysicsObject>& obj) {
                if (!obj->getIsActive()) return true;
                
                // 飞船不能离开战场，小行星可以
                if (dynamic_cast<Spaceship*>(obj.get())) {
                    obj->isOutOfBounds(screenWidth, screenHeight);
                    return false;
                } else {
                    if (obj->isOutOfBounds(screenWidth, screenHeight)) return true;
                }
                return false;
            }),
            objects.end());
        
        // 更新空间分区
        updateSpatialGrid();
        
        // 更新所有物体
        for (auto& obj : objects) {
            if (obj->getIsActive()) {
                obj->update(deltaTime);
            }
        }
        
        // 处理飞船自动攻击
        for (auto& obj : objects) {
            if (!obj->getIsActive()) continue;
            
            if (auto ship = std::dynamic_pointer_cast<Spaceship>(obj)) {
                // 寻找攻击目标
                for (auto& target : objects) {
                    if (target == obj || !target->getIsActive()) continue;
                    
                    if (ship->tryAttack(target)) {
                        break; // 一次只攻击一个目标
                    }
                }
            }
        }
        
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
                auto fragments = (*it)->onDestroy();
                objectsToAdd.insert(objectsToAdd.end(), fragments.begin(), fragments.end());
                it = objects.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void draw(sf::RenderWindow& window) {
        for (auto& obj : objects) {
            if (obj->getIsActive()) {
                obj->draw(window);
            }
        }
    }
    
    void clear() {
        objects.clear();
        objectsToAdd.clear();
    }
    
    size_t getObjectCount() const {
        return objects.size();
    }
};

// ====================== 游戏主类 ======================
class Game {
private:
    sf::RenderWindow window;
    sf::Clock clock;
    sf::Clock gameClock;
    PhysicsSystem physicsSystem;
    
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
          rng(std::random_device()()),
          distPos(0, 1),
          distVel(5, 40),  // 降低速度范围
          distSize(15, 35),
          asteroidTimer(0),
          timeSinceLastWave(0),
          waveCount(0) {
        
        // 开启抗锯齿
        sf::ContextSettings settings;
        settings.antialiasingLevel = 8;
        window.setFramerateLimit(60);
        
        // 初始化飞船
        initializeShips();
    }
    
    void initializeShips() {
        float screenWidth = 1280.0f;
        float screenHeight = 720.0f;
        float margin = 80.0f;
        
        // 计算屏幕中心
        Vector2D center(screenWidth / 2, screenHeight / 2);
        
        // 红色飞船 - 左上角
        auto redShip = std::make_shared<Spaceship>(
            Vector2D(margin, margin), sf::Color(255, 50, 50));
        // 朝向中心
        Vector2D toCenter1 = center - redShip->getPosition();
        redShip->setRotation(std::atan2(toCenter1.y, toCenter1.x) + PI/2);
        redShip->setAngularVelocity(0);
        physicsSystem.addObject(redShip);
        
        // 蓝色飞船 - 右上角
        auto blueShip = std::make_shared<Spaceship>(
            Vector2D(screenWidth - margin, margin), sf::Color(50, 150, 255));
        Vector2D toCenter2 = center - blueShip->getPosition();
        blueShip->setRotation(std::atan2(toCenter2.y, toCenter2.x) + PI/2);
        blueShip->setAngularVelocity(0);
        physicsSystem.addObject(blueShip);
        
        // 绿色飞船 - 左下角
        auto greenShip = std::make_shared<Spaceship>(
            Vector2D(margin, screenHeight - margin), sf::Color(50, 255, 100));
        Vector2D toCenter3 = center - greenShip->getPosition();
        greenShip->setRotation(std::atan2(toCenter3.y, toCenter3.x) + PI/2);
        greenShip->setAngularVelocity(0);
        physicsSystem.addObject(greenShip);
        
        // 黄色飞船 - 右下角
        auto yellowShip = std::make_shared<Spaceship>(
            Vector2D(screenWidth - margin, screenHeight - margin), sf::Color(255, 255, 50));
        Vector2D toCenter4 = center - yellowShip->getPosition();
        yellowShip->setRotation(std::atan2(toCenter4.y, toCenter4.x) + PI/2);
        yellowShip->setAngularVelocity(0);
        physicsSystem.addObject(yellowShip);
    }
    
    // 生成小行星的函数
    void generateAsteroidWave() {
        float screenWidth = 1280.0f;
        float screenHeight = 720.0f;
        
        // 波次间隔时间越长，小行星越多
        float baseCount = 2.0f;
        float timeFactor = std::min(10.0f, timeSinceLastWave / 30.0f); // 每30秒增加一个
        int asteroidCount = static_cast<int>(baseCount + timeFactor);
        
        // 偶尔（5%概率）生成大量小行星
        if (rand() % 100 < 5) {
            asteroidCount += 5 + rand() % 10;
        }
        
        for (int i = 0; i < asteroidCount; i++) {
            addRandomAsteroid();
        }
        
        timeSinceLastWave = 0;
        waveCount++;
    }
    
    void addRandomAsteroid() {
        float screenWidth = 1280.0f;
        float screenHeight = 720.0f;
        
        // 随机从左边或右边进入（90%概率），偶尔从上或下进入（10%概率）
        Vector2D position;
        Vector2D velocity;
        
        int entrySide = rand() % 10;
        
        if (entrySide < 4) {  // 40%从左边进入
            position = Vector2D(-50, distPos(rng) * screenHeight);
            velocity = Vector2D(distVel(rng) * 0.8f, (distPos(rng) - 0.5f) * 20.0f);
        } else if (entrySide < 8) {  // 40%从右边进入
            position = Vector2D(screenWidth + 50, distPos(rng) * screenHeight);
            velocity = Vector2D(-distVel(rng) * 0.8f, (distPos(rng) - 0.5f) * 20.0f);
        } else if (entrySide == 8) {  // 10%从上边进入
            position = Vector2D(distPos(rng) * screenWidth, -50);
            velocity = Vector2D((distPos(rng) - 0.5f) * 20.0f, distVel(rng) * 0.8f);
        } else {  // 10%从下边进入
            position = Vector2D(distPos(rng) * screenWidth, screenHeight + 50);
            velocity = Vector2D((distPos(rng) - 0.5f) * 20.0f, -distVel(rng) * 0.8f);
        }
        
        // 随机大小（大多数中等大小，少数小型高速，少数大型低速）
        float size;
        if (rand() % 100 < 70) {  // 70%中等大小
            size = 20.0f + distSize(rng) * 0.5f;
            velocity = velocity * 0.7f;  // 中等速度
        } else if (rand() % 100 < 85) {  // 15%大型低速
            size = 30.0f + distSize(rng);
            velocity = velocity * 0.4f;
        } else {  // 15%小型高速
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
                if (event.key.code == sf::Keyboard::Escape)
                    window.close();
                
                // R键重置
                if (event.key.code == sf::Keyboard::R) {
                    physicsSystem.clear();
                    initializeShips();
                    asteroidTimer = 0;
                    timeSinceLastWave = 0;
                    waveCount = 0;
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
        physicsSystem.update(deltaTime, 1280, 720);
        
        // 生成小行星波次
        asteroidTimer += deltaTime;
        
        // 波次间隔：基础5秒，随着时间增加逐渐缩短到2秒
        float baseInterval = 5.0f;
        float minInterval = 2.0f;
        float interval = std::max(minInterval, baseInterval - static_cast<float>(waveCount) * 0.1f);
        
        if (asteroidTimer >= interval) {
            generateAsteroidWave();
            asteroidTimer = 0;
        }
    }
    
    void render() {
        window.clear(sf::Color::Black);
        
        // 绘制所有物理对象
        physicsSystem.draw(window);
        
        window.display();
    }
};

// ====================== 主函数 ======================
int main() {
    // 设置随机种子
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // 彻底隐藏控制台
    #ifdef _WIN32
        HWND hwnd = GetConsoleWindow();
        if (hwnd) {
            ShowWindow(hwnd, SW_HIDE);
        }
    #endif
    
    // 创建游戏实例
    Game game(1280, 720);
    game.run();
    
    return 0;
}