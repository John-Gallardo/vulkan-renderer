static const float2 g_triangleVertices[3] = {
    {0, -0.5},    // top
    {0.5, 0.5},   // bottom-right
    {-0.5, 0.5}  // bottom-left
};

static const float4 g_colors[3] = {
    {1.0, 0.0, 0.0, 1.0},
    {0.0, 1.0, 0.0, 1.0},
    {0.0, 0.0, 1.0, 1.0}
};

[shader("vertex")]
float4 vertexMain(uint vertexID : SV_VertexID, out float4 color) : SV_Position {
    color = g_colors[vertexID];
    return float4(g_triangleVertices[vertexID], 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain(float4 color) : SV_Target {
    return color;
}
