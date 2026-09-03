#include "collisions.h"

#include "../graphics/graphics.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


std::vector<TriAABB> colliders;

// CLAUDE GENERATED FUNCTION
// Closest point to `p` on triangle t. The branches walk the triangle's Voronoi
// regions, so this is correct whether the nearest feature is the face interior,
// one of the three edges, or one of the three corners — which is exactly the
// distinction a box test can't make and why it runs second.
static glm::vec3 closestPointOnTriangle(const glm::vec3& p, const Tri& t)
{
    glm::vec3 ab = t.b - t.a, ac = t.c - t.a, ap = p - t.a;

    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return t.a;                   // corner A

    glm::vec3 bp = p - t.b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return t.b;                     // corner B

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)                 // edge AB
        return t.a + ab * (d1 / (d1 - d3));

    glm::vec3 cp = p - t.c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return t.c;                     // corner C

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)                 // edge AC
        return t.a + ac * (d2 / (d2 - d6));

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)   // edge BC
        return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));

    float denom = 1.0f / (va + vb + vc);                        // face interior
    return t.a + ab * (vb * denom) + ac * (vc * denom);
}

static glm::vec3 closestPointOnSegment(const glm::vec3& p,
                                       const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float abLenSq = glm::dot(ab, ab);
    if (abLenSq < 1e-12f) return a; // degenerate segment

    glm::vec3 ap = p - a;
    // projects ap onto ab but clamps the amount of ab between 0 and 1 and adds that to a
    return a + ab * glm::clamp(glm::dot(ap, ab) / abLenSq, 0.0f, 1.0f);
}

static const float GROUND_NORMAL_Y = 0.7f;

void resolveCapsuleCollision(Capsule*& capsule, const std::vector<TriAABB>& colliders)
{
    const float radiusSq = capsule->radius * capsule->radius;
    capsule->grounded = false;
    // adjust multiple times per frame so if an adjustment pushes capsule into a
    // wall or something then it will fix itself before the player sees anything.
    for (int i = 0; i < 4; ++i)
    {
        // rebuild the player box for every adjustment
        glm::vec3 feet(capsule->transform.x, capsule->transform.y, capsule->transform.z);
        AABB capsuleBox;
        capsuleBox.min = feet - glm::vec3(capsule->radius, 0.0f, capsule->radius);
        capsuleBox.max = feet + glm::vec3(capsule->radius, capsule->height, capsule->radius);
        capsuleBox.expand(0.1f);

        bool hitAny = false;

        for (const TriAABB& t : colliders)
        {
            if (!capsuleBox.overlaps(t.aabb)) continue;

            glm::vec3 rawNormal = glm::cross(t.b - t.a, t.c - t.a);
            float normalLenSq = glm::dot(rawNormal, rawNormal);
            if (normalLenSq < 1e-12f) continue; // degenerate tri
            
            // doesnt sqrt until after the degen check for slight optimization.
            glm::vec3 faceNormal = rawNormal / glm::sqrt(normalLenSq);

            glm::vec3 feet(capsule->transform.x, capsule->transform.y, capsule->transform.z);
            glm::vec3 base = feet + glm::vec3(0.0f, capsule->radius, 0.0f);
            glm::vec3 tip  = feet + glm::vec3(0.0f, capsule->height - capsule->radius, 0.0f);
            if (tip.y < base.y) tip = base; // degenerate to a sphere if capsule is wider than tall


            // this section finds the closest point to the tri on the segment between
            // base and tip of the capsule so we can treat it like a sphere.
            glm::vec3 axis = tip - base;
            float denom = glm::dot(faceNormal, axis);

            glm::vec3 reference;
            if (glm::abs(denom) < 1e-6f)
                reference = t.a; // axis parallel : any point serves
            else
                // gets closest point on segment to triangles plane
                reference = base + axis * glm::clamp(glm::dot(faceNormal, t.a-base) / denom,
                                                     0.0f, 1.0f);
                
            reference = closestPointOnTriangle(reference, t);

            // center is the closest point on the segment.
            glm::vec3 center = closestPointOnSegment(reference, base, tip);
            // contact is the closest point to center on the tri.
            glm::vec3 contact = closestPointOnTriangle(center, t);

            glm::vec3 delta = center - contact;
            float distSq = glm::dot(delta, delta);
            if (distSq >= radiusSq) continue; // doesnt collide

            glm::vec3 pushDir;
            float depth;
            if (distSq > 1e-12f)
            {
                float dist = glm::sqrt(distSq);
                pushDir = delta / dist;
                depth = capsule->radius - dist;
            } else {
                // center of sphere is on the surface revert to
                // normal because delta carries no direction.
                pushDir = faceNormal;
                depth = capsule->radius;
            }

            capsule->transform.y += pushDir.y * depth;

            if (pushDir.y > GROUND_NORMAL_Y) {
                capsule->grounded = true;
                // only cancel the component of motion heading straight down into the surface
                capsule->velocity.y -= pushDir.y * glm::min(0.0f, capsule->velocity.y * pushDir.y);
            } else {
                // only push player on x and z if its not a ground tri
                capsule->transform.x += pushDir.x * depth;
                capsule->transform.z += pushDir.z * depth;

                // cancel the component of motion heading into the surface
                capsule->velocity -= pushDir * glm::min(0.0f, glm::dot(capsule->velocity, pushDir));
            }

            hitAny = true;
        }

        if (!hitAny) break;
    }
}

void Object::CollectColliders(const glm::mat4 parentWorld, std::vector<TriAABB>& out)
{
    glm::mat4 w = parentWorld * transform.matrix();
    for (std::unique_ptr<Object>& child : children) child->CollectColliders(w, out);
}

static AABB triBounds(const Tri& t)
{
    AABB box;
    box.min = glm::min(glm::min(t.a, t.b), t.c);
    box.max = glm::max(glm::max(t.a, t.b), t.c);
    return box;
}

void StaticMesh::CollectColliders(const glm::mat4 parentWorld, std::vector<TriAABB>& out)
{
    glm::mat4 w = parentWorld * transform.matrix();
    const std::vector<unsigned int>& indices = this->getIndices();
    const std::vector<float>& vertices = this->getVertices();

    if (this->getCollides())
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            glm::vec3 corner[3];
            for (int c = 0; c < 3; ++c)
            {
                size_t v = (size_t)indices[i + c] * VERTEX_FLOATS;
                glm::vec3 local(vertices[v], vertices[v + 1], vertices[v + 2]);
                corner[c] = glm::vec3(w * glm::vec4(local, 1.0f));
            }
            TriAABB t{corner[0], corner[1], corner[2]};
            t.aabb = triBounds(Tri{t.a, t.b, t.c});
            out.push_back(t);
        }
    }
}

void collectSceneColliders()
{
    colliders.clear();
    for (std::unique_ptr<Object>& obj : rootObjs) obj->CollectColliders(glm::mat4(1.0f), colliders);
}

void Capsule::Compose()
{
    Object::Compose();
}   