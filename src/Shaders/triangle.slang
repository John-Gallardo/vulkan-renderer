static const float2 g_triangleVertices[3] = {
    {0, -0.5},    // top
    {0.5, 0.5},   // bottom-right
    {-0.5, 0.5}  // bottom-left
};

[shader("vertex")]
float4 vertexMain(uint vertexID : SV_VertexID) : SV_Position {
    return float4(g_triangleVertices[vertexID], 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain() : SV_Target {
    return {1.0, 1.0, 1.0, 1.0};
}
